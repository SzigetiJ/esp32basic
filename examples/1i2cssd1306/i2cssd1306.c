/*
 * Copyright 2024 SZIGETI János
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
#include "typeaux.h"
#include "esp_attr.h"
#include "ssd1306.h"

// =================== Hard constants =================

// #1: Timings -- 50ms: 20Hz update freq.
#define OLED_PERIOD_MS      50U   ///< SSD1306 update period
#define UARTCTRL_PERIOD_MS 500U

// #2: Channels / wires / addresses
#define I2C0_SCL_GPIO 22U
#define I2C0_SDA_GPIO 23U

#define OLED_I2C_FREQ_HZ 400000U

#define OLED_I2C_CH I2C0
#define OLED_I2C_SLAVEADDR 0x3c

// ============= Local types ===============

// ================ Local function declarations =================
static void _i2c_release_cycle(uint64_t u64tckNow);
static ELockmgrResource _i2c_to_lock(EI2CBus eBus);
static void _uartctrl_cycle(uint64_t u64tckNow);
static void _oled_cycle(uint64_t u64tckNow);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static bool gbOledInitialized = false;
static uint8_t gau8OledTxBuffer[64];
static uint8_t gu8OledTxBufferLen = 0;

// ==================== Implementation ================
// -------------- Internal functions --------------

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

static ELockmgrResource _i2c_to_lock(EI2CBus eBus) {
  return eBus;
}

static void _uartctrl_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static uint8_t u8DisplayOffset = 0;
  static uint8_t u8StartLine = 0;
  static uint8_t u8Brightness = 0x7F;

  if (u64tckNext <= u64tckNow) {
    gu8OledTxBufferLen = 1;
    gau8OledTxBuffer[0] = 0;
    while (0 < (gsUART0.STATUS & 0xff)) {
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      switch (cCtrl) {
        case 's':
          u8DisplayOffset-=2;
        case 'w' : // display offset up
        ++u8DisplayOffset;
        gu8OledTxBufferLen +=ssd1306_set_display_offset(&gau8OledTxBuffer[gu8OledTxBufferLen], u8DisplayOffset);
        break;
        case 'i':
          u8StartLine-=2;
        case 'k':
          ++u8StartLine;
        gu8OledTxBufferLen +=ssd1306_set_display_startline(&gau8OledTxBuffer[gu8OledTxBufferLen], u8StartLine);
        break;
        case '+':
          u8Brightness+=2;
        case '-':
          --u8Brightness;
        gu8OledTxBufferLen +=ssd1306_set_contrast_control(&gau8OledTxBuffer[gu8OledTxBufferLen], u8Brightness);
        break;
        default:
          uart_printf(&gsUART0, "command not found\r\n");
      }
    }
    if (gu8OledTxBufferLen == 1) {
      gu8OledTxBufferLen = 0;
    }
    u64tckNext+=MS2TICKS(UARTCTRL_PERIOD_MS);
  }
}

static void _oled_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static uint8_t acDataSeq[] = {0, 0, 0, 0x40, 0xFF, 0, 0xAA, 0};
  static uint32_t u32LastLabel;
  static bool bFirstRun = true;
  static uint8_t u8FilledColumns = 0;

  if (u64tckNext <= u64tckNow) {
    uint32_t u32NextLabel;
    if (lockmgr_acquire_lock(_i2c_to_lock(OLED_I2C_CH), &u32NextLabel)) {
      if (!bFirstRun) {
        AsyncResultEntry *psEntry = lockmgr_get_entry(u32LastLabel);
        bool bErr = (0 < (psEntry->u32IntSt & I2C_INT_MASK_ERR));
        if (!bErr) {
          if (!gbOledInitialized) {
            gbOledInitialized = true;
          }
        }
        lockmgr_release_entry(u32LastLabel);
      }
      bFirstRun = false;
      u32LastLabel = u32NextLabel;

      if (!gbOledInitialized) {
        uint8_t au8Dat[30];
        uint8_t u8DatLen = ssd1306_get_startseq(au8Dat);
        i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, u8DatLen, (const uint8_t*) au8Dat);
      } else {
        if (u8FilledColumns < 128) {
          i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(acDataSeq) - 3, (const uint8_t*) acDataSeq + 3);
          ++u8FilledColumns;
        } else { // blink
          if (0 < gu8OledTxBufferLen) {
            i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, gu8OledTxBufferLen, gau8OledTxBuffer);
            gu8OledTxBufferLen = 0;
          } else {
            uint32_t *pu32Data = &((uint32_t*)acDataSeq)[1];
            ++*pu32Data;
            i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(acDataSeq) - 3, (const uint8_t*) acDataSeq + 3);
          }
          u64tckNext += MS2TICKS(OLED_PERIOD_MS);
        }
      }
    }
  }
}


// -------------- Interface functions --------------

void prog_init_pro_pre() {
  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV.u20ClkDiv = APB_FREQ_HZ / 115200;

  lockmgr_init();
  i2c_init_controller(OLED_I2C_CH, I2C0_SCL_GPIO, I2C0_SDA_GPIO, HZ2APBTICKS(OLED_I2C_FREQ_HZ));
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
  _oled_cycle(u64tckNow);
}
