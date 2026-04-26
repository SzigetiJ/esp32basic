/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "esp_attr.h"
#include "dport.h"
#include "gpio.h"
#include "i2c.h"
#include "main.h"
#include "pidctrl.h"
#include "print.h"
#include "defines.h"
#include "romfunctions.h"
#include "rtc.h"
#include "timg.h"
#include "uart.h"
#include "iomux.h"
#include "xtutils.h"
#include "lockmgr.h"
#include "typeaux.h"
#include "bme280.h"
#include "bh1750.h"
#include "sht4x.h"
#include "ssd1306.h"
#include "utils/i2cutils.h"
#include "utils/uartutils.h"

// =================== Hard constants =================
// #1: Timings
#define LED_BLINK_HPERIOD0_MS 500U
#define LED_BLINK_HPERIOD1_MS 250U
#define OLED_PERIOD_MS 100U
#define BH1750_PERIOD_MS 1333U
#define BME280_PERIOD_MS 5200U
#define SHT40_PERIOD_MS 4800U
#define LOG_PERIOD_MS 4000U
#define INC_PERIOD_MS 1900U
#define I2CSCAN_PERIOD_MS 8600U
#define ALARM_PERIOD_MS 4500U
#define UARTCTRL_PERIOD_MS 503U

#define BH1750_RETRY_WAIT_HMS 10U

// #2: Channels / wires / addresses
#define I2C1_SCL_GPIO 22U
#define I2C1_SDA_GPIO 23U

#define OLED_I2C_FREQ_HZ 400000U

#define OLED_I2C_CH I2C1
#define OLED_I2C_SLAVEADDR 0x3c

#define BH1750_I2C_CH I2C1
#define BH1750_I2C_SLAVEADDR 0x23

#define BME280_I2C_CH I2C1
#define BME280_I2C_SLAVEADDR 0x76

#define SHT40_I2C_CH I2C1
#define SHT40_I2C_SLAVEADDR 0x44

// #3: Sizes
#define UART0_TXSIZE 1U
#define I2CSCAN_PRINT_PER_ROW 8

// #4: Others
#define BH1750_READ_RETRIES 5U

// ============= Local types ===============

typedef struct {
  Isr pfCallback;
  void *pvParam;
} InterruptEntry;

typedef enum {
  DISPLAY_INIT,
  DISPLAY_CLRSCR,
  DISPLAY_NORMAL
} EDisplayState;

typedef enum {
  BH1750_PH_INIT,
  BH1750_PH_RESET,
  BH1750_PH_MEASURE,
  BH1750_PH_READ
} EBh1750Phase;

typedef struct {
  InterruptEntry sRoutine;
  uint64_t u64tckAlarmCur;
  uint32_t u32tckAlarmPeriod;
  int8_t u8Int;
  TimerId sTimer;
  ECpu eCpu;
} PeriodicCallbackDesc;

// ================ Local function declarations =================
static void  _uart_print_header(UART_Type *psUart, uint64_t u64tckNow, const char* strModuleName);
static void _flush_message(uint64_t u64tckNow);
static void _alternate_value(void *pvParam);
static ELockmgrResource _i2c_to_lock(EI2CBus eBus);
static void _schedule_isr();
static void _i2cscan_cycle(uint64_t u64tckNow);
static void _init_drivers();
static void _init_uart();
static void _i2c_release_cycle(uint64_t u64tckNow);
static void _switch_leds_init(TimerId sTimer);
static void _switch_leds_cycle(uint64_t u64tckNow);
static void _oled_cycle(uint64_t u64tckNow);
static void _bh1750_init(SBh1750StateDesc *psState, SI2cIfaceCfg *psIface);
static void _bh1750_print_result(uint64_t u64tckNow, const SBh1750StateDesc *psState);
static void _bh1750_cycle(uint64_t u64tckNow);
static void _bme280_init(SBme280StateDesc *psState, SI2cIfaceCfg *psIface);
static void _bme280_print_result(uint64_t u64tckNow, const SBme280TPH *psRes, uint32_t u32TFine);
static void _bme280_cycle(uint64_t u64tckNow);
static void _sht40_print_result(uint64_t u64tckNow, SSht40StateDesc *psState);
static void _sht40_cycle(uint64_t u64tckNow);
static void _log_cycle(uint64_t u64tckNow);
static void _inc_cycle(uint64_t u64tckNow);
static void _uartctrl_cycle(uint64_t u64tckNow);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const char acLedPhase[] = "*O";

static UART_Type *gpsUART0 = &gsUART0;
static UART_Type *gpsUART0M = &gsUART0Mapped; // for FIFO read
static volatile bool gbLedState = false;
static volatile EDisplayState geOledState = DISPLAY_INIT;
static volatile uint64_t gu64tckAlarmCur = 0;
static volatile uint32_t gau32IncVal[] = {0, 0, 0, 0};
static volatile uint32_t gu32MutexIncProc = 0;
static const uint8_t gau8LedGpio [] = {2, 4};
char gacOledDataSeq[] = {// shift = 3
  0x00, 0x00, 0x00, 0x40, // data sequence begins
  0xAA, 0xAA, 0xAA, 0xAA
};

static PeriodicCallbackDesc gsPCbDesc = {
  //  .eCpu = CPU_PRO,
  .sRoutine =
  {
    .pfCallback = _alternate_value,
    .pvParam = (void*) &gbLedState
  },
  //  .sTimer = gsLedTimer,
  .u8Int = 24,
  .u32tckAlarmPeriod = MS2TICKS(ALARM_PERIOD_MS),
  .u64tckAlarmCur = 0
};

// ============== Implementation ==============
// -------------- Internal functions --------------

static void _uart_print_header(UART_Type *psUart, uint64_t u64tckNow, const char* strModuleName) {
  uart_printf(psUart, "[%d:%s]", (uint32_t)(u64tckNow / TICKS_PER_MS), strModuleName);
}

static void _flush_message(uint64_t u64tckNow) {
  uint64_t u64TsDecimal = u64tckNow / TICKS_PER_MS; // milliseconds, max. ~ 48 bits.
  uint32_t u32TsDecimalHi = u64TsDecimal / 1000; // seconds, FIXME: max. ~ 38 bits, does not fit into uint32_t
  uint32_t u32TsDecimalLo = u64TsDecimal % 1000; // ms part of the timestamp
  uint32_t u32TsFractional = (u64tckNow % TICKS_PER_MS) * (1000000 / TICKS_PER_MS); // µs and ns, 6 digits

  _uart_print_header(gpsUART0, u64tckNow, "LOGGER");
  uart_printf(gpsUART0, " ts: %d %03d.%06d ms\r\n", u32TsDecimalHi, u32TsDecimalLo, u32TsFractional);
}

/**
 * Takes a boolean parameter and changes it to its netaged valus.
 * @param pvParam Pointer to a boolean value.
 */
static void IRAM_ATTR _alternate_value(void *pvParam) {
  bool *pbParam = (bool*) pvParam;
  *pbParam = !*pbParam;
}

/**
 * Executes a callback with given parameter, next, registers itself for getting called repeatedly (periodically).
 * We do not use the AUTO-RELOAD at ALARM capability of TIMG, as the timer may be use for several other purposes.
 * @param pvParam Ptr to PeriodicCallbackDesc structure, storing all the data required for this function.
 */
static void IRAM_ATTR _timer_isr(void *pvParam) {
  PeriodicCallbackDesc *psParam = (PeriodicCallbackDesc*) pvParam;
  gapsTIMG[psParam->sTimer.eTimg]->INT_CLR_TIMERS |= 1 << psParam->sTimer.eTimer;
  psParam->sRoutine.pfCallback(psParam->sRoutine.pvParam);
  psParam->u64tckAlarmCur += psParam->u32tckAlarmPeriod;
  timg_callback_at(psParam->u64tckAlarmCur, psParam->eCpu, psParam->sTimer, psParam->u8Int, &_timer_isr, pvParam);
}

static ELockmgrResource _i2c_to_lock(EI2CBus eBus) {
  return eBus;
}

static void _init_drivers() {
  lockmgr_init();
  i2c_init_controller(OLED_I2C_CH, I2C1_SCL_GPIO, I2C1_SDA_GPIO, HZ2APBTICKS(OLED_I2C_FREQ_HZ));
}

static void _init_uart() {
  gpsUART0->CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);
  uart_set_memconf_xsize(gpsUART0, true, UART0_TXSIZE);
}

// TODO: make it an ISR and attach to I2C INT

static void _i2c_release_cycle(uint64_t u64tckNow) {
  ELockmgrResource eBus = _i2c_to_lock(OLED_I2C_CH);
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

// LED blinking

static void _switch_leds_init(TimerId sTimer) {
  gsPCbDesc.sTimer = sTimer;
  for (int i = 0; i < ARRAY_SIZE(gau8LedGpio); ++i) {
    gpio_pin_enable(gau8LedGpio[i]);
  }
}

static void _switch_leds_cycle(uint64_t u64tckNow) {
  static bool bPhase = false;
  static uint64_t u64tckNext = 0;
  static RegAddr aprGpioOut[] = {&gsGPIO.OUT_W1TS, &gsGPIO.OUT_W1TC};

  if (u64tckNext <= u64tckNow) {
    gpio_reg_setbit(aprGpioOut[bPhase], gau8LedGpio[0]);
    gpio_reg_setbit(aprGpioOut[!bPhase], gau8LedGpio[1]);
    if (false) {
      gpsUART0->FIFO = acLedPhase[bPhase];
    }
    bPhase = !bPhase;
    u64tckNext += MS2TICKS(gbLedState ? LED_BLINK_HPERIOD1_MS : LED_BLINK_HPERIOD0_MS);
  }
}

// 32x128 display

static void _oled_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static uint32_t u32Value0 = 0;
  static uint32_t u32Value1 = 0;
  static uint32_t u32Mul0 = 1;
  static uint32_t u32Div0 = 6;
  static uint32_t u32Mul1 = 3;
  static uint32_t u32Div1 = 7;
  static uint32_t u32ClrSrcPtr = 0;
  static uint32_t u32LastLabel;
  static bool bFirstRun = true;

  if (u64tckNext <= u64tckNow) {
    uint32_t u32NextLabel;
    if (lockmgr_acquire_lock(_i2c_to_lock(OLED_I2C_CH), &u32NextLabel)) {
      if (!bFirstRun) {
        AsyncResultEntry *psEntry = lockmgr_get_entry(u32LastLabel);
        bool bErr = (0 < (psEntry->u32IntSt & I2C_INT_MASK_ERR));
        if (!bErr) {
          if (geOledState == DISPLAY_INIT) {
            geOledState = DISPLAY_CLRSCR;
          } else if (geOledState == DISPLAY_CLRSCR) {
            ++u32ClrSrcPtr;
            if (u32ClrSrcPtr == 256) {
              geOledState = DISPLAY_NORMAL;
            }
          }
        }
        lockmgr_release_entry(u32LastLabel);
      }
      bFirstRun = false;
      u32LastLabel = u32NextLabel;

      switch (geOledState) {
        case DISPLAY_INIT:
          uint8_t au8Dat[30];
          uint8_t u8DatLen = ssd1306_get_startseq(au8Dat);
          i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, u8DatLen, (const uint8_t*) au8Dat);
          break;
        case DISPLAY_CLRSCR:
          i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(gacOledDataSeq) - 3, (const uint8_t*) gacOledDataSeq + 3);
          break;
        default:
        {
          uint8_t u8X0 = (u32Value0 * u32Mul0 / u32Div0) & 0x1F;
          uint8_t u8X1 = 31 - ((u32Value1 * u32Mul1 / u32Div1) & 0x1F);
          uint32_t u32Pattern = u8X1 < u8X0 ? ((1 << u8X0) - (1 << u8X1)) : ~((1 << u8X1) - (1 << u8X0));
          *((uint32_t*) (&gacOledDataSeq[4])) = u32Pattern;

          i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(gacOledDataSeq) - 3, (const uint8_t*) gacOledDataSeq + 3);

          u64tckNext += MS2TICKS(OLED_PERIOD_MS);
          ++u32Value0;
          if (32U * u32Div0 <= u32Value0) {
            u32Value0 = 0;
          }
          ++u32Value1;
          if (32U * u32Div1 <= u32Value1) {
            u32Value1 = 0;
          }
        }
      }
    }
  }
}

// Section BME280

static void _bme280_init(SBme280StateDesc *psState, SI2cIfaceCfg *psIface) {
  *psState = bme280_init_state();
  bme280_set_osrs(psState, BME280_SEL_T, BME280_OSRS_8);
  bme280_set_osrs(psState, BME280_SEL_P, BME280_OSRS_8);
  bme280_set_osrs(psState, BME280_SEL_H, BME280_OSRS_8);
  bme280_set_mode(psState, BME280_MODE_FORCED);
  *psIface = (SI2cIfaceCfg){
    .eBus = BME280_I2C_CH,
    .eLck = _i2c_to_lock(BME280_I2C_CH),
    .u8SlaveAddr = BME280_I2C_SLAVEADDR
  };
}

static void _bme280_print_result(uint64_t u64tckNow, const SBme280TPH *psRes, uint32_t u32TFine) {
  _uart_print_header(gpsUART0, u64tckNow, "BME280");
  uart_printf(gpsUART0, "\r\n  Tfine: %d\r\n", u32TFine);
  uart_printf(gpsUART0, "  Temp: %d.%02d\r\n", psRes->i32Temp / 100, psRes->i32Temp % 100);
  uart_printf(gpsUART0, "  Pres: %d.%02d\r\n", psRes->i32Pres >> 8, ((psRes->i32Pres & 0xff) * 391) / 1000);
  uart_printf(gpsUART0, "  Hum: %d.%03d\r\n", psRes->i32Hum >> 10, ((psRes->i32Hum & 0x3ff) * 97657) / 100000);
}

static void _bme280_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = MS2TICKS(BME280_PERIOD_MS);
  static bool bFirstRun = true;
  static SBme280StateDesc sState;
  static SI2cIfaceCfg sIface;

  if (bFirstRun) {
    _bme280_init(&sState, &sIface);
    bFirstRun = false;
  }

  if (u64tckNext <= u64tckNow) {
    uint32_t u32hmsWaitHint = 0;
    bme280_async_rx_cycle(&sState, &u32hmsWaitHint);
    if (bme280_is_data_ready(&sState)) {
      uint32_t u32TFine;
      SBme280TPH sResult = bme280_get_measurement(&sState, &u32TFine);
      _bme280_print_result(u64tckNow, &sResult, u32TFine);
      bme280_req_data(&sState);
      bme280_set_mode(&sState, BME280_MODE_FORCED);
      u64tckNext += MS2TICKS(BME280_PERIOD_MS);
    } else {
      if (u32hmsWaitHint == 0) {
        bme280_async_tx_cycle(&sIface, &sState);
      } else {
        u64tckNext += MS2TICKS(u32hmsWaitHint) / 2;
      }
    }
  }
}

// Section SHT40

static void _sht40_print_result(uint64_t u64tckNow, SSht40StateDesc *psState) {
  _uart_print_header(gpsUART0, u64tckNow, "SHT40");
  uart_printf(gpsUART0, " cmd: #%u, raw: %02X %02X %02X %02X %02X %02X,",
          psState->eCommand,
          psState->au8RxBuffer[0], psState->au8RxBuffer[1], psState->au8RxBuffer[2],
          psState->au8RxBuffer[3], psState->au8RxBuffer[4], psState->au8RxBuffer[5]);
  if (psState->eCommand != SHT4X_CMD_SERIAL) {
    int32_t i32TempM = sht4x_get_temp(psState);
    int32_t i32HumM = sht4x_get_hum(psState);
    uart_printf(gpsUART0, " Temp: %d.%03d, Hum: %d.%03d\r\n", i32TempM / 1000, i32TempM % 1000, i32HumM / 1000, i32HumM % 1000);
  } else {
    uint32_t u32Serial = sht4x_get_serial(psState);
    uart_printf(gpsUART0, " Serial number: %08X\r\n", u32Serial);
  }
  // check crc
  if (!sht4x_check_crc(psState, true)) {
    uart_printf(gpsUART0, " 1st CRC8 does not match (Temp)\r\n");
  }
  if (!sht4x_check_crc(psState, true)) {
    uart_printf(gpsUART0, " 2nd CRC8 does not match (Hum)\r\n");
  }
}

static void _sht40_cycle(uint64_t u64tckNow) {
  static ESht4xCommand eCommand = SHT4X_CMD_MEAS_H;
  static uint64_t u64tckNext = MS2TICKS(SHT40_PERIOD_MS);
  static bool bFirstRun = true;
  static SSht40StateDesc sState;

  if (bFirstRun) {
    sState = sht40_init_descriptor((SI2cIfaceCfg){SHT40_I2C_CH, SHT40_I2C_SLAVEADDR, _i2c_to_lock(SHT40_I2C_CH)});
    bFirstRun = false;
  }

  uint32_t u32msWait = 0;

  if (u64tckNext <= u64tckNow) {
    if (sState.eState == SHT4X_STATE_READY || sState.eState == SHT4X_STATE_ERROR) {
      if (sState.eState == SHT4X_STATE_ERROR) {
        _uart_print_header(gpsUART0, u64tckNow, "SHT40");
        uart_printf(gpsUART0, " ERROR!");
      } else {
        if (sState.eCommand != SHT4X_CMD_RESET) {
          _sht40_print_result(u64tckNow, &sState);
        }
      }
      u32msWait = SHT40_PERIOD_MS;
      ++eCommand;
      if (SHT4X_CMD_RESET < eCommand) { // here we skip SHT4X_CMD_HEAT* commands
        eCommand = SHT4X_CMD_MEAS_H;
      }
      sState.eState = SHT4X_STATE_IDLE;
    } else if (sState.eState == SHT4X_STATE_IDLE) {
      sht40_set_command(&sState, eCommand);
    }

    if (sht40_needs_rxtx(&sState)) {
      u32msWait = sht4x_rxtx_cycle(&sState);
    }
    u64tckNext += MS2TICKS(u32msWait);
  }
}

// Section BH1750FVI

static void _bh1750_init(SBh1750StateDesc *psState, SI2cIfaceCfg *psIface) {
  *psState = bh1750_init_state();
  *psIface = (SI2cIfaceCfg){
    .eBus = BME280_I2C_CH,
    .eLck = _i2c_to_lock(BH1750_I2C_CH),
    .u8SlaveAddr = BH1750_I2C_SLAVEADDR
  };
}

static void _bh1750_print_result(uint64_t u64tckTimestamp, const SBh1750StateDesc *psState) {
  static const char *acBh1750MResName[] = {
    "H", "H2", "XX", "L"
  };
  EBh1750MeasRes eMRes = bh1750_get_mres(psState);
  uint8_t u8MTime = bh1750_get_mtime(psState);
  uint16_t u16Result = conv16be(psState->u16beResult);
  uint32_t u32mLx = bh1750_result_to_mlx(u16Result, u8MTime, eMRes);
  uint32_t u32hmsMTime = bh1750_measurementtime_hms(u8MTime, eMRes);

  _uart_print_header(gpsUART0, u64tckTimestamp, "BH1750");
  uart_printf(gpsUART0, " mode: %s, result: %d.%03d lx (raw: %u), mtime: %u ms (raw: %u)\r\n",
          acBh1750MResName[eMRes],
          u32mLx / 1000, u32mLx % 1000, u16Result,
          u32hmsMTime / 2, u8MTime);
}

static void _bh1750_cycle(uint64_t u64tckNow) {

  static uint64_t u64tckNext = MS2TICKS(BH1750_PERIOD_MS);
  static SBh1750StateDesc sState;
  static SI2cIfaceCfg sIface;
  static EBh1750Phase ePhase = BH1750_PH_INIT;
  static uint8_t u8Retries = BH1750_READ_RETRIES;
  static uint8_t u8MTime = BH1750_MTIME_DEFAULT;

  if (u64tckNext <= u64tckNow) {
    if (ePhase == BH1750_PH_INIT) {
      _bh1750_init(&sState, &sIface);
    }
    uint32_t u32hmsWaitHint = 0;
    bool bResultReady = false;
    bool bSeqReady = bh1750_async_rx_cycle(&sState, &u32hmsWaitHint);
    if (bSeqReady) {
      switch (ePhase) {
        case BH1750_PH_MEASURE:
          // switch to read
          ePhase = BH1750_PH_READ;
          bh1750_read(&sState);
          break;
        case BH1750_PH_READ:
          // check result and conditionally switch to reset
          if (0 != sState.u16beResult || 0 == u8Retries--) {
            ePhase = BH1750_PH_RESET;
            bh1750_reset(&sState); // in case of one-time measurement this command implies POWER_ON
            bResultReady = true;
            u8Retries = BH1750_READ_RETRIES;
          } else {
            // EITHER 0 was measured OR result still not ready
            _uart_print_header(gpsUART0, u64tckNow, "BH1750");
            uart_printf(gpsUART0, " retry\r\n");
            u32hmsWaitHint += BH1750_RETRY_WAIT_HMS; // so let's wait some dt
          }
          break;
        case BH1750_PH_RESET: // result register is reset
        case BH1750_PH_INIT: // there was no action before
          // switch to measure
          ePhase = BH1750_PH_MEASURE;
          bh1750_measure(&sState, false, bh1750_measres_next(bh1750_get_mres(&sState)));
          if (bh1750_get_mres(&sState) == BH1750_RES_H) {
            do {
              u8MTime += 5;
            } while (u8MTime < BH1750_MTIME_MIN || BH1750_MTIME_MAX < u8MTime);
            bh1750_set_mtime(&sState, u8MTime);
          }
          break;
        default:
          ;
      }
    }
    if (bResultReady) {
      _bh1750_print_result(u64tckNow, &sState);
      u64tckNext += MS2TICKS(BH1750_PERIOD_MS);
    } else { // TX side
      if (u32hmsWaitHint == 0) {
        bh1750_async_tx_cycle(&sIface, &sState);
      } else {
        u64tckNext += MS2TICKS(u32hmsWaitHint) / 2;
      }
    }
  }
}


// Logger

static void _log_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;

  if (u64tckNext <= u64tckNow) {
    _flush_message(u64tckNow);
    u64tckNext += MS2TICKS(LOG_PERIOD_MS);
  }
}

// value incrementation on two cores with mutex

static void _inc_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext[] = {0, 0};

  uint32_t au32Tmp[ARRAY_SIZE(gau32IncVal)];
  uint32_t u32CurrentCore = xt_utils_get_core_id();
  uint8_t u8CurrentCore = u32CurrentCore ? 1 : 0;

  if (u64tckNext[u8CurrentCore] <= u64tckNow) {
    while (!xt_utils_compare_and_set(&gu32MutexIncProc, 0, u32CurrentCore + 1));
    for (int i = 0; i < 1000; ++i) {
      for (int j = 0; j < ARRAY_SIZE(gau32IncVal); ++j) {
        au32Tmp[j] = gau32IncVal[j];
      }
      for (int j = 0; j < ARRAY_SIZE(gau32IncVal); ++j) {
        ++au32Tmp[j];
      }
      for (int j = 0; j < ARRAY_SIZE(gau32IncVal); ++j) {
        gau32IncVal[ARRAY_SIZE(gau32IncVal) - j - 1] = au32Tmp[ARRAY_SIZE(gau32IncVal) - j - 1];
      }
    }
    gu32MutexIncProc = 0;
    u64tckNext[u8CurrentCore] += MS2TICKS(INC_PERIOD_MS);
  }
}

static void _i2cscan_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static SI2cScanStateDesc sState;
  static SI2cIfaceCfg sIface;
  static bool bFirstRun = true;

  if (bFirstRun) {
    sState = i2cutil_scan_init();
    sIface.eBus = OLED_I2C_CH;
    sIface.eLck = _i2c_to_lock(OLED_I2C_CH);
    bFirstRun = false;
  }

  if (u64tckNext <= u64tckNow) {
    if (i2cutils_scan_cycle(&sIface, &sState)) {
      uint8_t u8DevCnt = 0;
      _uart_print_header(gpsUART0, u64tckNow, "I2CScan");
      for (uint8_t i = 0; i < 128; ++i) {
        if (sState.au8Slave[i / 8] & (1 << (i % 8))) {
          if (u8DevCnt == 0) {
            uart_printf(gpsUART0, " I2C slaves found:");
          }
          uart_printf(gpsUART0, "%s0x%02X", (u8DevCnt % I2CSCAN_PRINT_PER_ROW) ? " " : "\r\n  ", i);
          ++u8DevCnt;
        }
      }
      if (u8DevCnt == 0) {
        uart_printf(gpsUART0, " no devices found.");
      }
      uart_printf(gpsUART0, "\r\n");

      u64tckNext += MS2TICKS(I2CSCAN_PERIOD_MS);
      sState = i2cutil_scan_init();
    }
  }
}

static void _uartctrl_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckWakeup = 0;
  static char cCommand = ' '; // init value not used.
  static char acArg[2]; // a set of commands require argument(s). They are store in this array (reverse order).
  static uint8_t u8WaitForArgs = 0;  // number of arguments the current command still requires.

  if (u64tckWakeup <= u64tckNow) {
    while (0 < (gpsUART0->STATUS & 0xff)) {
      char cCtrl = gpsUART0M->FIFO & 0xff;
      if (0 < u8WaitForArgs) {
        acArg[--u8WaitForArgs] = cCtrl;
        if (0 == u8WaitForArgs) { // all the required number of arguments arrived
          switch (cCommand) {
            case 'w': // put a byte into lockmgr RX buffer
              uint8_t u8ArgValue = char_to_hex8(acArg[0]) | (char_to_hex8(acArg[1]) << 4);
              ELockmgrResource eRes = _i2c_to_lock(OLED_I2C_CH);
              bool bLocked = lockmgr_is_locked(eRes);
              if (bLocked) {
                uint32_t u32Label = lockmgr_get_lock_owner(eRes);
                AsyncResultEntry* psEntry = lockmgr_get_entry(u32Label);
                psEntry->pu8ReceiveBuffer[0] = u8ArgValue;
                ++psEntry->pu8ReceiveBuffer;
                --psEntry->u8RxLen;
              }

              break;
          }
        }
      } else {
        switch (cCtrl) {
          case 'h': // help
            _uart_print_header(gpsUART0, u64tckNow, "CTRL");
            uart_printf(gpsUART0, "\r\n [h]\tprint help %d\r\n", uart_tx_cnt(gpsUART0));
            uart_printf(gpsUART0, " [i]\tshow I2C status %d\r\n", uart_tx_cnt(gpsUART0));
            uart_printf(gpsUART0, " [l]\tshow I2C lock state %d\r\n", uart_tx_cnt(gpsUART0));
            uart_printf(gpsUART0, " [wXX]\tput a byte into current lockmgr RX buffer\r\n");
            uart_printf(gpsUART0, " [r]\trelease current lock\r\n");
            break;
          case 'i': // I2C status
            _uart_print_header(gpsUART0, u64tckNow, "CTRL");
            uart_printf(gpsUART0, " GPIO_FUNC_OUT: %08X %08X", gpio_regs()->FUNC_OUT_SEL_CFG[I2C1_SCL_GPIO], gpio_regs()->FUNC_OUT_SEL_CFG[I2C1_SDA_GPIO]);
            uart_printf(gpsUART0, "\tI2C Regs: %08X %08X %08X %08X\r\n", i2c_regs(I2C0)->SR, i2c_regs(I2C0)->FIFO_CONF, i2c_regs(I2C0)->INT_RAW, i2c_regs(I2C0)->INT_ST);
            break;
          case 'l': // show locks
          {
            ELockmgrResource eRes = _i2c_to_lock(OLED_I2C_CH);
            bool bLocked = lockmgr_is_locked(eRes);
            _uart_print_header(gpsUART0, u64tckNow, "CTRL");
            uart_printf(gpsUART0, " locked: %d", bLocked);
            if (bLocked) {
              uint32_t u32Label = lockmgr_get_lock_owner(eRes);
              AsyncResultEntry* psEntry = lockmgr_get_entry(u32Label);
              uart_printf(gpsUART0, ", label: %d, bytes: %d, INT: %08X", u32Label, psEntry->u8RxLen, psEntry->u32IntSt);
            }
            uart_printf(gpsUART0, "\r\n");
          }
            break;
          case 'r':
          {
            _uart_print_header(gpsUART0, u64tckNow, "CTRL");
            uart_printf(gpsUART0, "release lock\r\n");
            ELockmgrResource eRes = _i2c_to_lock(OLED_I2C_CH);
            bool bLocked = lockmgr_is_locked(eRes);
            if (bLocked) {
              uint32_t u32Label = lockmgr_get_lock_owner(eRes);
              AsyncResultEntry* psEntry = lockmgr_get_entry(u32Label);
              psEntry->bReady = true;
              lockmgr_free_lock(eRes);
            }
          }
            break;
          case 'w':
            cCommand = 'w';
            u8WaitForArgs = 2;
            break;
          default:
            _uart_print_header(gpsUART0, u64tckNow, "CTRL");
            uart_printf(gpsUART0, " `%c' command not recognized\r\n", cCtrl);
        }
      }
    }
    u64tckWakeup += MS2TICKS(UARTCTRL_PERIOD_MS);
  }
}

static void _schedule_isr() {
  gsPCbDesc.eCpu = xt_utils_get_core_id() ? CPU_APP : CPU_PRO;
  timg_callback_at(gsPCbDesc.u64tckAlarmCur, gsPCbDesc.eCpu, gsPCbDesc.sTimer, gsPCbDesc.u8Int, &_timer_isr, (void*) &gsPCbDesc);
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  _init_uart();
  _schedule_isr();
}

void prog_init_app() {
  _init_drivers();
}

void prog_init_pro_post() {
  TimerId sTimer = {0, 0};
  _switch_leds_init(sTimer);
}

void prog_cycle_app(uint64_t u64tckNow) {
  _i2c_release_cycle(u64tckNow);
  _inc_cycle(u64tckNow);
  _oled_cycle(u64tckNow);
}

// user tasks begin

void prog_cycle_pro(uint64_t u64tckNow) {
  _inc_cycle(u64tckNow);
  _switch_leds_cycle(u64tckNow);
  _log_cycle(u64tckNow);
  _i2cscan_cycle(u64tckNow);
  _bh1750_cycle(u64tckNow);
  _bme280_cycle(u64tckNow);
  _sht40_cycle(u64tckNow);
  _uartctrl_cycle(u64tckNow);
}
