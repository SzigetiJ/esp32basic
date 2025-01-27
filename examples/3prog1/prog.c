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
#include "utils/i2cutils.h"

// =================== Hard constants =================
// #1: Timings
#define LED_BLINK_HPERIOD0_MS 500U
#define LED_BLINK_HPERIOD1_MS 250U
#define OLED_PERIOD_MS 100U
#define BH1750_PERIOD_MS 1333U
#define BME280_PERIOD_MS 5200U
#define LOG_PERIOD_MS 4000U
#define INC_PERIOD_MS 1900U
#define I2CSCAN_PERIOD_MS 8600U
#define ALARM_PERIOD_MS 4500U

#define BH1750_RETRY_WAIT_HMS 10U

// #2: Channels / wires / addresses
#define I2C0_SCL_GPIO 22U
#define I2C0_SDA_GPIO 23U

#define OLED_I2C_FREQ_HZ 400000U

#define OLED_I2C_CH I2C1
#define OLED_I2C_SLAVEADDR 0x3c

#define BH1750_I2C_CH I2C1
#define BH1750_I2C_SLAVEADDR 0x23

#define BME280_I2C_CH I2C1
#define BME280_I2C_SLAVEADDR 0x76

// #3: Sizes
#define LOG_BUFLEN 120
#define I2CSCAN_PRINT_PER_ROW 8

// #4: Others
#define BH1750_READ_RETRIES 5U

// ============= Local types ===============

typedef struct {
  Isr pfCallback;
  void *pvParam;
} InterruptEntry;

typedef struct {
  uint8_t coreId;
  uint8_t id;
  uint8_t prio;
} InterruptDesc;

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
static void _uart_println(const char *pcPrefix, const char *pcLine, uint8_t u8Len);
static void _flush_message(uint64_t u64tckTimestamp);
static void _alternate_value(void *pvParam);
static ELockmgrResource _i2c_to_lock(EI2CBus eBus);
static void _schedule_isr();
static void _i2cscan_cycle(uint64_t u64Ticks);
static void _init_drivers();
static void _init_uart();
static void _i2c_release_cycle(uint64_t u64Ticks);
static void _switch_leds_init(TimerId sTimer);
static void _switch_leds_cycle(uint64_t u64Ticks);
static void _oled_cycle(uint64_t u64Ticks);
static void _bh1750_init(SBh1750StateDesc *psState, SI2cIfaceCfg *psIface);
static void _bh1750_print_result(const SBh1750StateDesc *psState);
static void _bh1750_cycle(uint64_t u64Ticks);
static void _bme280_init(SBme280StateDesc *psState, SI2cIfaceCfg *psIface);
static void _bme280_print_result(const SBme280TPH *psRes, uint32_t u32TFine);
static void _bme280_cycle(uint64_t u64Ticks);
static void _log_cycle(uint64_t u64Ticks);
static void _inc_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const char message_pfx[] = "LOG:\tts: ";
static const char message_sfx[] = " ms\r\n";
static const char acLedPhase[] = "*O";

static UART_Type *gpsUART0 = &gsUART0;
static volatile bool gbLedState = false;
static volatile EDisplayState geOledState = DISPLAY_INIT;
static volatile uint64_t gu64tckAlarmCur = 0;
static volatile uint32_t gau32IncVal[] = {0, 0, 0, 0};
static volatile uint32_t gu32MutexIncProc = 0;
static const uint8_t gau8LedGpio [] = {2, 4};
const char gacOledStartSeq[] = {
  0x00, // command sequence begins
  0xA8, 0x3F, 0xD3, 0x00, // Set MUX ratio, Set display offset
  0x40, 0x20, 0x01, 0xA0, // Set display start line, Set segment re-map
  0xC0, 0xDA, 0x02, // Set COM Output scan direction, Set COM pins hw config
  0x81, 0x0F, // Set contrast ctrl
  0xA4, 0xA6, // Disable entire display ON, Set normal display,
  0xD5, 0x80, 0x8D, 0x14, // Set OSC frequency, Enable charge pump regulator
  0xAF, // Display on
  0x00, 0x10, // Reset Column
  0x22, 0x00, 0x03 // Addressing page range [0,3]
};
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


// Implementation

static void _uart_println(const char *pcPrefix, const char *pcLine, uint8_t u8Len) {
  uint32_t u32PfxLen = strlen(pcPrefix);
  for (int i = 0; i < u32PfxLen; ++i) {
    gpsUART0->FIFO = pcPrefix[i];
  }
  for (int i = 0; i < u8Len; ++i) {
    gpsUART0->FIFO = pcLine[i];
  }
  gpsUART0->FIFO = '\r';
  gpsUART0->FIFO = '\n';
}

static void _flush_message(uint64_t u64tckTimestamp) {
  static uint8_t u8Phase;
  static char buf[LOG_BUFLEN];
  char *buf_e = buf;

  uint64_t u64TsDecimal = u64tckTimestamp / TICKS_PER_MS; // milliseconds, max. ~ 48 bits.
  uint32_t u32TsDecimalHi = u64TsDecimal / 1000; // seconds, FIXME: max. ~ 38 bits, does not fit into uint32_t
  uint32_t u32TsDecimalLo = u64TsDecimal % 1000; // ms part of the timestamp
  uint32_t u32TsFractional = (u64tckTimestamp % TICKS_PER_MS) * (1000000 / TICKS_PER_MS); // µs and ns, 6 digits

  if (true) {
    buf_e = print_dec(buf_e, u32TsDecimalHi);
    *(buf_e++) = ' ';
    buf_e = print_dec_padded(buf_e, u32TsDecimalLo, 3, '0');
    *(buf_e++) = '.';
    buf_e = print_dec_padded(buf_e, u32TsFractional, 6, '0');
    buf_e = str_append(buf_e, " ms");
    for (int i = 0; i < 0; ++i) {
      *(buf_e++) = ' ';
      buf_e = print_dec(buf_e, gau32IncVal[i]);
    }
    if (false) {
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, gpio_regs()->FUNC_OUT_SEL_CFG[I2C0_SCL_GPIO]);
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, gpio_regs()->FUNC_OUT_SEL_CFG[I2C0_SDA_GPIO]);
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, i2c_regs(I2C0)->SR);
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, i2c_regs(I2C0)->FIFO_CONF);
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, i2c_regs(I2C0)->INT_RAW);
      *(buf_e++) = ' ';
      buf_e = print_hex32(buf_e, i2c_regs(I2C0)->INT_ST);
      *(buf_e++) = ' ';
      buf_e = print_hex8(buf_e, u8Phase & 0x0f);
      *(buf_e++) = ':';
      buf_e = print_hex32(buf_e, i2c_regs(I2C0)->COMD[u8Phase & 0x0f]);
    }
  } else {
    // some internal call causes WDT
    snprintf(buf, LOG_BUFLEN, "%s%"PRIu64"%s", message_pfx, u64tckTimestamp, message_sfx);
  }
  _uart_println("LOG:\tts ", buf, buf_e - buf);
  ++u8Phase;
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
  i2c_init_controller(OLED_I2C_CH, I2C0_SCL_GPIO, I2C0_SDA_GPIO, HZ2APBTICKS(OLED_I2C_FREQ_HZ));
}

static void _init_uart() {
  gpsUART0->CLKDIV.u20ClkDiv = APB_FREQ_HZ / UART_FREQ_HZ;
}

// TODO: make it an ISR and attach to I2C INT

static void _i2c_release_cycle(uint64_t u64Ticks) {
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

static void _switch_leds_cycle(uint64_t u64Ticks) {
  static bool bPhase = false;
  static uint64_t u64NextTick = 0;
  static RegAddr aprGpioOut[] = {&gsGPIO.OUT_W1TS, &gsGPIO.OUT_W1TC};

  if (u64NextTick <= u64Ticks) {
    gpio_reg_setbit(aprGpioOut[bPhase], gau8LedGpio[0]);
    gpio_reg_setbit(aprGpioOut[!bPhase], gau8LedGpio[1]);
    if (false) {
      gpsUART0->FIFO = acLedPhase[bPhase];
    }
    bPhase = !bPhase;
    u64NextTick += MS2TICKS(gbLedState ? LED_BLINK_HPERIOD1_MS : LED_BLINK_HPERIOD0_MS);
  }
}

// 32x128 display

static void _oled_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static uint32_t u32Value0 = 0;
  static uint32_t u32Value1 = 0;
  static uint32_t u32Mul0 = 1;
  static uint32_t u32Div0 = 6;
  static uint32_t u32Mul1 = 3;
  static uint32_t u32Div1 = 7;
  static uint32_t u32ClrSrcPtr = 0;
  static uint32_t u32LastLabel;
  static bool bFirstRun = true;

  if (u64NextTick <= u64Ticks) {
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
          i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(gacOledStartSeq), (const uint8_t*) gacOledStartSeq);
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

          u64NextTick += MS2TICKS(OLED_PERIOD_MS);
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
  bme280_set_osrs_h(psState, BME280_OSRS_8);
  bme280_set_osrs_t(psState, BME280_OSRS_8);
  bme280_set_osrs_p(psState, BME280_OSRS_8);
  bme280_set_mode_forced(psState);
  *psIface = (SI2cIfaceCfg){
    .eBus = BME280_I2C_CH,
    .eLck = _i2c_to_lock(BME280_I2C_CH),
    .u8SlaveAddr = BME280_I2C_SLAVEADDR
  };
}

static void _bme280_print_result(const SBme280TPH *psRes, uint32_t u32TFine) {
  char acBuf[20];
  char *bufE;
  bufE = print_dec(acBuf, u32TFine);
  _uart_println("Tfine: ", acBuf, bufE - acBuf);
  bufE = print_deccent(acBuf, psRes->i32Temp, '.');
  _uart_println("Temp: ", acBuf, bufE - acBuf);
  bufE = print_dec(acBuf, psRes->i32Pres >> 8);
  *(bufE++) = '.';
  bufE = print_dec_padded(bufE, ((psRes->i32Pres & 0xff)*391) / 1000, 2, '0');
  _uart_println("Pres: ", acBuf, bufE - acBuf);
  bufE = print_dec(acBuf, psRes->i32Hum >> 10);
  *(bufE++) = '.';
  bufE = print_dec_padded(bufE, ((psRes->i32Hum & 0x3ff)*97657) / 100000, 3, '0');
  _uart_println("Hum: ", acBuf, bufE - acBuf);
}

static void _bme280_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(BME280_PERIOD_MS);
  static bool bFirstRun = true;
  static SBme280StateDesc sState;
  static SI2cIfaceCfg sIface;

  if (bFirstRun) {
    _bme280_init(&sState, &sIface);
    bFirstRun = false;
  }

  if (u64NextTick <= u64Ticks) {
    uint32_t u32hmsWaitHint = 0;
    bme280_async_rx_cycle(&sState, &u32hmsWaitHint);
    if (bme280_is_data_updated(&sState)) {
      uint32_t u32TFine;
      SBme280TPH sResult = bme280_get_measurement(&sState, &u32TFine);
      _bme280_print_result(&sResult, u32TFine);
      bme280_ack_data_updated(&sState);
      bme280_set_mode_forced(&sState);
      u64NextTick += MS2TICKS(BME280_PERIOD_MS);
    } else {
      if (u32hmsWaitHint == 0) {
        bme280_async_tx_cycle(&sIface, &sState);
      } else {
        u64NextTick += MS2TICKS(u32hmsWaitHint) / 2;
      }
    }
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

static void _bh1750_print_result(const SBh1750StateDesc *psState) {
  static const char *acBh1750MResName[] = {
    "H", "H2", "XX", "L"
  };
  EBh1750MeasRes eMRes = bh1750_get_mres(psState);
  uint8_t u8MTime = bh1750_get_mtime(psState);
  uint8_t u16Result = conv16be(psState->u16beResult);
  uint32_t u32mLx = bh1750_result_to_mlx(u16Result, u8MTime, eMRes);
  char acBuf[40];
  char *pcBufE = acBuf;
  pcBufE = str_append(pcBufE, acBh1750MResName[eMRes]);
  pcBufE = str_append(pcBufE, ": ");
  pcBufE = print_decmilli(pcBufE, u32mLx, '.');

  _uart_println("BH1750 ", acBuf, pcBufE - acBuf);
}

static void _bh1750_cycle(uint64_t u64Ticks) {
  const uint8_t u8MTimeMin = 31;
  const uint8_t u8MTimeMax = 254;

  static uint64_t u64NextTick = MS2TICKS(BH1750_PERIOD_MS);
  static SBh1750StateDesc sState;
  static SI2cIfaceCfg sIface;
  static EBh1750Phase ePhase = BH1750_PH_INIT;
  static uint8_t u8Retries = BH1750_READ_RETRIES;
  static uint8_t u8MTime = 69;

  if (u64NextTick <= u64Ticks) {
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
            _uart_println("BH1750 retry", NULL, 0);
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
            } while (u8MTime < u8MTimeMin || u8MTimeMax < u8MTime);
            bh1750_set_mtime(&sState, u8MTime);
          }
          break;
        default:
          ;
      }
    }
    if (bResultReady) {
      _bh1750_print_result(&sState);
      u64NextTick += MS2TICKS(BH1750_PERIOD_MS);
    } else { // TX side
      if (u32hmsWaitHint == 0) {
        bh1750_async_tx_cycle(&sIface, &sState);
      } else {
        u64NextTick += MS2TICKS(u32hmsWaitHint) / 2;
      }
    }
  }
}


// Logger

static void _log_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;

  if (u64NextTick <= u64Ticks) {
    _flush_message(u64Ticks);
    u64NextTick += MS2TICKS(LOG_PERIOD_MS);
  }
}

// value incrementation on two cores with mutex

static void _inc_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick[] = {0, 0};

  uint32_t au32Tmp[ARRAY_SIZE(gau32IncVal)];
  uint32_t u32CurrentCore = xt_utils_get_core_id();
  uint8_t u8CurrentCore = u32CurrentCore ? 1 : 0;

  if (u64NextTick[u8CurrentCore] <= u64Ticks) {
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
    u64NextTick[u8CurrentCore] += MS2TICKS(INC_PERIOD_MS);
  }
}

static void _i2cscan_cycle(uint64_t u64Ticks) {
  const char acPfx[] = "I2C slave(s) found:";
  static uint64_t u64NextTick = 0;
  static SI2cScanStateDesc sState;
  static SI2cIfaceCfg sIface;
  static bool bFirstRun = true;

  if (bFirstRun) {
    sState = i2cutil_scan_init();
    sIface.eBus = OLED_I2C_CH;
    sIface.eLck = _i2c_to_lock(OLED_I2C_CH);
    bFirstRun = false;
  }

  if (u64NextTick <= u64Ticks) {
    if (i2cutils_scan_cycle(&sIface, &sState)) {
      char acBuf[5 * I2CSCAN_PRINT_PER_ROW + 2];
      char *pcBufE = acBuf;
      for (uint8_t i = 0; i < 128; ++i) {
        if (sState.au8Slave[i / 8] & (1 << (i % 8))) {
          pcBufE = str_append(pcBufE, " 0x");
          pcBufE = print_hex8(pcBufE, i);
          if (5 * I2CSCAN_PRINT_PER_ROW <= (pcBufE - acBuf)) {
            _uart_println(acPfx, acBuf, pcBufE - acBuf);
            pcBufE = acBuf;
          }
        }
      }
      if (pcBufE != acBuf) {
        _uart_println(acPfx, acBuf, pcBufE - acBuf);
      }

      u64NextTick += MS2TICKS(I2CSCAN_PERIOD_MS);
      sState = i2cutil_scan_init();
    }
  }
}

static void _schedule_isr() {
  gsPCbDesc.eCpu = xt_utils_get_core_id() ? CPU_APP : CPU_PRO;
  timg_callback_at(gsPCbDesc.u64tckAlarmCur, gsPCbDesc.eCpu, gsPCbDesc.sTimer, gsPCbDesc.u8Int, &_timer_isr, (void*) &gsPCbDesc);
}

// ====================== Interface functions =========================

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
}
