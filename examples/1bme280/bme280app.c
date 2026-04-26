/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include "dport.h"
#include "gpio.h"
#include "main.h"
#include "defines.h"
#include "romfunctions.h"
#include "uart.h"
#include "utils/uartutils.h"
#include "i2c.h"
#include "lockmgr.h"
#include "iomux.h"
#include "dport.h"
#include "timg.h"
#include "typeaux.h"
#include "esp_attr.h"
#include "bme280.h"
#include "utils/i2cutils.h"

// =================== Hard constants =================

// #1: Timings -- 50ms: 20Hz update freq.
#define UART_FREQ_HZ    115200U
#define I2C0_FREQ_HZ    400000U
#define I2CSCAN_PERIOD_MS 5050U
#define BME280_TSTARTUP_MS   2U

#define UARTCTRL_PERIOD_MS 100U

// #2: Channels / wires / addresses
#define I2C0_SCL_GPIO   23U
#define I2C0_SDA_GPIO   22U
#define BME280_CSB_GPIO 21U

#define BME280_I2C_CH  I2C1
#define BME280_I2C_SLAVEADDR 0x76

// ============= Local types ===============


// ================ Local function declarations =================
static void _i2c_release_cycle(uint64_t u64tckNow);
static ELockmgrResource _i2c_to_lock(EI2CBus eBus);
static void _bme280_init(SBme280StateDesc *psState, SI2cIfaceCfg *psIface);
static void _bme280_print_result(uint64_t u64tckNow, const SBme280TPH *psRes, uint32_t u32TFine, uint8_t abValid);
static void _bme280_cycle(uint64_t u64tckNow, const SI2cIfaceCfg *psIface, SBme280StateDesc *psState);
static bool _modify_osrs(int8_t i8Diff, EBme280MetricSelector eMetric, SBme280StateDesc *psState);
static void _uartctrl_cycle(uint64_t u64tckNow);

void _i2c_error(void *pvParam);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

const uint32_t gau32msForcedPeriod[] = {
  100,
  200,
  500,
  1000,
  2000,
  5000,
  10000
};

// ==================== Local Data ================
static SBme280StateDesc gsState;
static bool gbDoubleWait = false; // BUGFIX: apparently, after modifying osrs_x value, the first measurement takes twice as much time as expected.
static bool gbVerbose = false;
static SI2cIfaceCfg gsIface;
static uint8_t gu8ForcedPeriodIdx = 4;

// ==================== Implementation ================
// -------------- Internal functions --------------

static void _i2c_release_cycle(uint64_t u64tckNow) {
  ELockmgrResource eBus = _i2c_to_lock(BME280_I2C_CH);
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);

  if (lockmgr_is_locked(eBus)) {
    bool bBusIdle = !i2c_isbusy(psI2C);
    if (bBusIdle) {
      uint32_t u32Label = lockmgr_get_lock_owner(eBus);
      AsyncResultEntry* psEntry = lockmgr_get_entry(u32Label);
      psEntry->u32IntSt = psI2C->INT_ST;
      if (0 < psEntry->u8RxLen) {
        for (int i = 0; i < psEntry->u8RxLen; ++i) {
          psEntry->pu8ReceiveBuffer[i] = (uint8_t) (prData[i] & 0xff);
        }
      }
      psEntry->bReady = true;
      lockmgr_free_lock(eBus);
    }
  }
}

static ELockmgrResource _i2c_to_lock(EI2CBus eBus) {
  return eBus;
}

static void _bme280_init(SBme280StateDesc *psState, SI2cIfaceCfg *psIface) {
  // chip select
  gpio_pin_enable(BME280_CSB_GPIO);
  gpio_pin_out_on(BME280_CSB_GPIO);

  // init state
  *psState = bme280_init_state();
  bme280_set_osrs(psState, BME280_SEL_H, BME280_OSRS_8);
  bme280_set_osrs(psState, BME280_SEL_P, BME280_OSRS_8);
  bme280_set_osrs(psState, BME280_SEL_T, BME280_OSRS_8);
  gbDoubleWait = true;
  bme280_set_config(psState, BME280_TSB_1000MS, BME280_IIR_OFF, 0);
  bme280_req_calib(psState);
  bme280_req_id(psState);

  *psIface = (SI2cIfaceCfg){
    .eBus = BME280_I2C_CH,
    .eLck = _i2c_to_lock(BME280_I2C_CH),
    .u8SlaveAddr = BME280_I2C_SLAVEADDR
  };
}

static void _bme280_print_result(uint64_t u64tckNow, const SBme280TPH *psRes, uint32_t u32TFine, uint8_t abValid) {
  uart_printf(&gsUART0, "[%d]", (uint32_t)(u64tckNow / TICKS_PER_MS));
  uart_printf(&gsUART0, "\tTfine: %d", u32TFine);
  if (abValid & 1)
    uart_printf(&gsUART0, "\tTemp: %d.%02d", psRes->i32Temp / 100, psRes->i32Temp % 100);
  if (abValid & 2)
    uart_printf(&gsUART0, "\tPres: %d.%02d", psRes->i32Pres >> 8, ((psRes->i32Pres & 0xff) * 391) / 1000);
  if (abValid & 4)
    uart_printf(&gsUART0, "\tHum: %d.%03d", psRes->i32Hum >> 10, ((psRes->i32Hum & 0x3ff) * 97657) / 100000);
  uart_printf(&gsUART0, "\r\n");
}

static void _bme280_cycle(uint64_t u64tckNow, const SI2cIfaceCfg *psIface, SBme280StateDesc *psState) {
  static uint64_t u64tckMainNext = 0;
  static uint64_t u64tckNext = MS2TICKS(BME280_TSTARTUP_MS);
  static uint8_t u8Phase = 0; // 0: idle, 1: set mode, 2: wait, 3: get status, 4: get data, 5: print data
  static uint32_t u32hmsMeasurementExp = 0; // expected measurement time [half ms]
  static uint32_t u32ExtraWaitCnt = 0;      // additional cycles after expected measurement time (status is not 0)

  if (u64tckNext < u64tckMainNext) {
    u64tckNext = u64tckMainNext;
  }
  if (u64tckNext <= u64tckNow) {
    uint32_t u32hmsWaitHint = 0;
    //    bool bRxRes = bme280_async_rx_cycle(psState, &u32hmsWaitHint);
    bme280_async_rx_cycle(psState, &u32hmsWaitHint);
    bool bOngoing = bme280_has_async_todo(psState);

    if (!bOngoing) {  // the bme280 todo queue is empty (nothing to set, nothing requested)
      ++u8Phase;

      if (u8Phase == 5) {
        if (gbVerbose) {
          uart_printf(&gsUART0, "Predicted dt: %u, status reties: %u\r\n", u32hmsMeasurementExp / 2, u32ExtraWaitCnt);
        }
        uint32_t u32TFine;
        SBme280TPH sMeas = bme280_get_measurement(psState, &u32TFine);
        _bme280_print_result(u64tckNow, &sMeas, u32TFine,
                (bme280_get_osrs(psState, true, BME280_SEL_T) ? 1 : 0) |
                (bme280_get_osrs(psState, true, BME280_SEL_P) ? 2 : 0) |
                (bme280_get_osrs(psState, true, BME280_SEL_H) ? 4 : 0)
                );
        u8Phase = 0;
      } else {
        if (u8Phase == 1) {
          bme280_set_mode(psState, BME280_MODE_FORCED);
          u32hmsMeasurementExp = (gbDoubleWait ? 2 : 1) * bme280_measurement_duration_hms(psState);
        } else if (u8Phase == 2) {
          u64tckNext = u64tckNow + HMS2TICKS(u32hmsMeasurementExp);
        } else if (u8Phase == 3) {
          gbDoubleWait = false;
          bme280_req_status(psState);
          u32ExtraWaitCnt = 0;
        } else if (u8Phase == 4) {
          uint8_t u8Status = bme280_get_status(psState) & 0x09;
          if (u8Status != 0) {
            gsUART0.FIFO = 'a' + u8Status;
            bme280_req_status(psState);
            --u8Phase;
            ++u32ExtraWaitCnt;
          } else {
            bme280_req_data(psState);
          }
        }
      }
    }
    // TX side
    bool bTxRes = bme280_async_tx_cycle(psIface, psState);
    bool bTodo = bme280_has_async_todo(psState);
    bool bWaitForRx = bme280_is_waiting(psState);

    // calculate wait time
    if (bTodo && !bTxRes) { // could not initialize TX, retry soon
      // do not increase nxt timestamp
    } else if (bWaitForRx) {
      if (((psState->u32CommState>>24)&0x0f)==6) {
        u64tckNext = u64tckNow + MS2TICKS(BME280_TSTARTUP_MS);
      }
      // do not increase nxt timestamp
    } else {  // no tx problem, not waiting for rx
      if (u8Phase == 0) { // find next main cycle tick
        u64tckMainNext += MS2TICKS(gau32msForcedPeriod[gu8ForcedPeriodIdx]);
      } else {
        // no wait
      }
    }
  }
}

static bool _modify_osrs(int8_t i8Diff, EBme280MetricSelector eMetric, SBme280StateDesc *psState) {
  EBme280Osrs eCurVal = bme280_get_osrs(psState, false, eMetric);
  const char acMetric2Ch[] = {'h', 'p', 't'};

  int8_t i8NewVal = eCurVal + i8Diff;
  bool bValid = (0 <= i8NewVal && i8NewVal < 8);
  if (!bValid) {
    i8NewVal = eCurVal;
  }
  int8_t i8Mul =
          i8NewVal == 0 ? 0 :
          5 <= i8NewVal ? 16 :
          1 << (i8NewVal - 1);
  if (bValid) {
    bme280_set_osrs(psState, eMetric, i8NewVal);
    uart_printf(&gsUART0, "osrs_%c modified: %u -> %d (x%d)\r\n", acMetric2Ch[eMetric], eCurVal, i8NewVal, i8Mul);
  } else {
    uart_printf(&gsUART0, "osrs_%c not changed: %d (x%d)\r\n", acMetric2Ch[eMetric], i8NewVal, i8Mul);
  }
  return bValid;
}

static void _modify_idx(int8_t i8IdxDiff, uint8_t *pu8Idx, uint8_t u8MaxIdx, const uint32_t *pu32Values, const char *pcParamName) {
  uint8_t u8OrigIdx = *pu8Idx;
  int8_t i8NewIdx = u8OrigIdx + i8IdxDiff;
  bool bValid = (0 <= i8NewIdx && i8NewIdx < u8MaxIdx);
  if (!bValid) {
    i8NewIdx = u8OrigIdx;
  }
  if (bValid) {
    *pu8Idx = i8NewIdx;
    uart_printf(&gsUART0, "%s modified: %u -> %u (#%d)\r\n", pcParamName, pu32Values[u8OrigIdx], pu32Values[i8NewIdx], i8NewIdx);
  } else {
    uart_printf(&gsUART0, "%s not changed: %u (#%u)\r\n", pcParamName, pu32Values[u8OrigIdx], u8OrigIdx);
  }
}

static void _uartctrl_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  int8_t i8OsrsTDiff = 0;
  int8_t i8OsrsPDiff = 0;
  int8_t i8OsrsHDiff = 0;
  int8_t i8TsbDiff = 0;
  int8_t i8TfpDiff = 0;   // forced mode period
  static bool bInReset = false;

  if (u64tckNext <= u64tckNow) {
    while (0 < (gsUART0.STATUS & 0xff)) {
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      switch (cCtrl) {
        case '-':
          --i8OsrsTDiff;
          break;
        case '+':
          ++i8OsrsTDiff;
          break;
        case '[':
          --i8OsrsPDiff;
          break;
        case ']':
          ++i8OsrsPDiff;
          break;
        case '{':
          --i8OsrsHDiff;
          break;
        case '}':
          ++i8OsrsHDiff;
          break;
        case '<':
          --i8TsbDiff;
          break;
        case '>':
          ++i8TsbDiff;
          break;
        case ',':
          --i8TfpDiff;
          break;
        case '.':
          ++i8TfpDiff;
          break;
        case 'c':
          bme280_req_config(&gsState);
          break;
        case 'i':
          uart_printf(&gsUART0, "ID: 0x%02X\t", bme280_get_id(&gsState));
          uart_printf(&gsUART0, "Setters: %02X\t", gsState.u32CommState&0xFF);
          uart_printf(&gsUART0, "Getters: %02X\t", (gsState.u32CommState>>8)&0xFF);
          uart_printf(&gsUART0, "CommFlags: %02X\r\n", (gsState.u32CommState>>16)&0xFF);
          uart_printf(&gsUART0, "Config/Status: %08X\t", ((uint32_t*)gsState.au8ConfigMirror)[0]);
          uart_printf(&gsUART0, "Raw Data: %08X.%08X\r\n", ((uint32_t*)gsState.au8DataMirror)[1], ((uint32_t*)gsState.au8DataMirror)[0]);
          break;
        case 'I':
          gbVerbose=!gbVerbose;
          uart_printf(&gsUART0, "Verbose mode: %u\r\n", gbVerbose);
          break;
        case 'r':
          bme280_reset(&gsState);
          bme280_req_calib(&gsState);
          bInReset = true;
          uart_printf(&gsUART0, "Resetting...");
          break;
        default:
          uart_printf(&gsUART0, "command not found\r\n");
      }
    }
    if (i8OsrsTDiff != 0) {
      gbDoubleWait |= _modify_osrs(i8OsrsTDiff, BME280_SEL_T, &gsState);
    }
    if (i8OsrsPDiff != 0) {
      gbDoubleWait |= _modify_osrs(i8OsrsPDiff, BME280_SEL_P, &gsState);
    }
    if (i8OsrsHDiff != 0) {
      gbDoubleWait |= _modify_osrs(i8OsrsHDiff, BME280_SEL_H, &gsState);
    }
    if (i8TfpDiff != 0) {
      _modify_idx(i8TfpDiff, &gu8ForcedPeriodIdx, ARRAY_SIZE(gau32msForcedPeriod), gau32msForcedPeriod, "Forced mode period (ms)");
    }
    if (bInReset) {
      if (bme280_is_resetting(&gsState)) {
        uart_printf(&gsUART0, ".");
      } else {
        uart_printf(&gsUART0, " Done.\r\n");
        bInReset = false;
      }
    }
    u64tckNext += MS2TICKS(UARTCTRL_PERIOD_MS);
  }
}


// -------------- Interface functions --------------

void prog_init_pro_pre() {
  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);

  lockmgr_init();
  i2c_init_controller(BME280_I2C_CH, I2C0_SCL_GPIO, I2C0_SDA_GPIO, HZ2APBTICKS(I2C0_FREQ_HZ));

  _bme280_init(&gsState, &gsIface);
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _i2c_release_cycle(u64tckNow);
  _uartctrl_cycle(u64tckNow);
  _bme280_cycle(u64tckNow, &gsIface, &gsState);
}
