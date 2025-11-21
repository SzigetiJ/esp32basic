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
#include "ssd1306.h"
#include "ascii8x8.h"

// =================== Hard constants =================

// #1: Timings -- 50ms: 20Hz update freq.
#define UART_FREQ_HZ    115200U
#define OLED_I2C_FREQ_HZ 400000U

#define OLED_INIT_DELAY_MS 100U  ///< seems like OLED requires some time to startup
#define OLED_PERIOD_MS      50U   ///< SSD1306 update period
#define UARTCTRL_PERIOD_MS 100U

// #2: Channels / wires / addresses
#define I2C0_SCL_GPIO 22U
#define I2C0_SDA_GPIO 23U

#define OLED_I2C_CH I2C0
#define OLED_I2C_SLAVEADDR 0x3c
#define I2C_INT_CH 23U

#define WAITCYCLES      5U      ///< wait (for command) cycles between data writes
#define COLS_TO_FILL  128U
#define PAGES_TO_FILL   4U
#define OLED_CHR_PAGES  8U      ///< number of pages in character mode
#define OLED_CHR_COLS 128U      ///< number of columns in character mode

// ============= Local types ===============

typedef struct {
  uint32_t u32Cur;
  uint32_t u32DatLen;
  uint8_t *pu8Dat;
  uint8_t u8LastEnd;
} FeedState;

typedef enum {
  OLED_INIT = 0,
  OLED_FILL_COLS,
  OLED_FILL_FULL,
  OLED_CMD_WRITE,
  OLED_DATA_WRITE
} EOledState;

typedef struct {
  bool bFirstRun;
  EOledState eState;
  uint32_t u32LastLabel;
  uint8_t u8FilledColumns;
  uint8_t u8CmdWriteCycle;
  uint32_t u32TotalDataWrites;
} SOledStateVariables;

// ================ Local function declarations =================
static void _i2c_release_cycle(uint64_t u64tckNow);
static ELockmgrResource _i2c_to_lock(EI2CBus eBus);
static void _uartctrl_cycle(uint64_t u64tckNow);
static void _oled_init();
static void _oled_inner_cycle(uint64_t u64tckNow, uint32_t u32NextLabel);
static void _oled_cycle(uint64_t u64tckNow);

void _i2c_feed(void *pvParam);
void _i2c_start(void *pvParam);
void _i2c_compl(void *pvParam);
void _i2c_error(void *pvParam);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const TimerId gsTimer = {.eTimg = TIMG_0, .eTimer = TIMER0};
static uint8_t gau8DataSeq2[PAGES_TO_FILL * COLS_TO_FILL + 1];
static uint8_t gau8OledTxBuffer[64];
static uint8_t gu8OledTxBufferLen = 1;
static FeedState gsOledTxState = {
  .pu8Dat = gau8OledTxBuffer,
  .u32Cur = 0,
  .u32DatLen = ARRAY_SIZE(gau8OledTxBuffer),
  .u8LastEnd = 0
};

static uint8_t gu8CursorChrPosX = 0;
static uint8_t gu8CursorChrPosY = 0;
static EAsciiCharset geCharset = ASCII_CHRSET_8x8;
static uint32_t gu32FeedCnt = 0;
static uint32_t gu32ComplFeed = 0;
static uint32_t gu32ErrorCnt = 0;
static uint64_t gu64tckOledI2CStart;

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
  static uint8_t u8MuxRatio = 0x3F;
  static bool bRotated = false;
  static bool bInverse = false;
  static bool bEntire = false;

  if (u64tckNext <= u64tckNow) {
    while (0 < (gsUART0.STATUS & 0xff)) {
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      switch (cCtrl) {
        case 's':
          u8DisplayOffset -= 2;
        case 'w': // display offset up
          ++u8DisplayOffset;
          gu8OledTxBufferLen += ssd1306_set_display_offset(&gau8OledTxBuffer[gu8OledTxBufferLen], u8DisplayOffset);
          break;
        case 'r': // rotate display by 180°
          bRotated = !bRotated;
          gu8OledTxBufferLen += ssd1306_set_segment_remap(&gau8OledTxBuffer[gu8OledTxBufferLen], bRotated);
          gu8OledTxBufferLen += ssd1306_set_output_scan_dir(&gau8OledTxBuffer[gu8OledTxBufferLen], bRotated);
          break;
        case 'i':
          u8StartLine -= 2;
        case 'k':
          ++u8StartLine;
          gu8OledTxBufferLen += ssd1306_set_display_startline(&gau8OledTxBuffer[gu8OledTxBufferLen], u8StartLine);
          break;
        case '+':
          u8Brightness += 2;
        case '-':
          --u8Brightness;
          gu8OledTxBufferLen += ssd1306_set_contrast_control(&gau8OledTxBuffer[gu8OledTxBufferLen], u8Brightness);
          break;
        case '9':
          u8MuxRatio -= 2;
        case '0':
          ++u8MuxRatio;
          if (0x3F < u8MuxRatio) u8MuxRatio = 0x0F;
          if (u8MuxRatio < 0x0F) u8MuxRatio = 0x3F;
          gu8OledTxBufferLen += ssd1306_set_mux_ratio(&gau8OledTxBuffer[gu8OledTxBufferLen], u8MuxRatio);
          break;
        case 'c': // select next charset
          ++geCharset;
          if (0 == ((1 << geCharset) & ascii_supported_charsets())) {
            geCharset = ASCII_CHRSET_8x8;
          }
          gu8CursorChrPosX = 0;
          gu8CursorChrPosY = 0;
          break;
        case 'x':
          bInverse = !bInverse;
          gu8OledTxBufferLen += ssd1306_inverse_display(&gau8OledTxBuffer[gu8OledTxBufferLen], bInverse);
          break;
        case 'e':
          bEntire = !bEntire;
          gu8OledTxBufferLen += ssd1306_entire_display_on(&gau8OledTxBuffer[gu8OledTxBufferLen], bEntire);
          break;
        default:
          uart_printf(&gsUART0, "command not found\r\n");
      }
    }
    u64tckNext += MS2TICKS(UARTCTRL_PERIOD_MS);
  }
}

static void _oled_init() {
  i2c_isr_register(OLED_I2C_CH, I2C_INT_TX_SEND_EMPTY, _i2c_feed, &gsOledTxState);
  i2c_isr_register(OLED_I2C_CH, I2C_INT_TRANS_START, _i2c_start, &gu64tckOledI2CStart);
  i2c_isr_register(OLED_I2C_CH, I2C_INT_TRANS_COMPL, _i2c_compl, &gu64tckOledI2CStart);
  // create the display full-fill pattern (diagonal stripes)
  const uint8_t au8Pattern[] = {0x99, 0x33, 0x66, 0xCC};
  ssd1306_ctrl(gau8DataSeq2, false, true);
  for (int i = 0; i < COLS_TO_FILL; ++i) {
    for (int j = 0; j < PAGES_TO_FILL; ++j) {
      gau8DataSeq2[1 + i * PAGES_TO_FILL + j] = au8Pattern[i % ARRAY_SIZE(au8Pattern)];
    }
  }
  // clear command buffer
  memset(gau8OledTxBuffer, 0x00, ARRAY_SIZE(gau8OledTxBuffer));
}

static void _oled_inner_cycle(uint64_t u64tckNow, uint32_t u32NextLabel) {
  const uint8_t au8ColFillData[] = {0x40, 0xFF, 0, 0xAA, 0};
  static uint8_t au8CursorChrData[8 + 1];
  static uint8_t u8CursorChrDataLen = 1;
  static SOledStateVariables sVar = {
    .bFirstRun = true,
    .eState = OLED_INIT,
    .u32LastLabel = 0,
    .u8CmdWriteCycle = 0,
    .u8FilledColumns = 0,
    .u32TotalDataWrites = 0
  };
  SAsciiAttributes sCharsetAttr = ascii_charset_attr(geCharset);

  if (!sVar.bFirstRun) {
    AsyncResultEntry *psEntry = lockmgr_get_entry(sVar.u32LastLabel);
    bool bErr = (0 < (psEntry->u32IntSt & I2C_INT_MASK_ERR));
    if (!bErr) {
      // do state change
      if ((sVar.eState == OLED_FILL_COLS && sVar.u8FilledColumns < COLS_TO_FILL)
              || (sVar.eState == OLED_CMD_WRITE && sVar.u8CmdWriteCycle < WAITCYCLES)) {
        // stay in current state
      } else if (sVar.eState == OLED_DATA_WRITE) {
        --sVar.eState;
      } else {
        ++sVar.eState;
      }
    } else { // error occurred
      // ?
    }
    lockmgr_release_entry(sVar.u32LastLabel);
  }
  sVar.bFirstRun = false;
  sVar.u32LastLabel = u32NextLabel;

  switch (sVar.eState) {
    case OLED_INIT:
    {
      uint8_t au8Dat[31]; // 31 bytes are the maximum a simple i2c write can handle natively
      uint8_t u8DatLen = ssd1306_get_startseq(au8Dat);
      i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, u8DatLen, (const uint8_t*) au8Dat);
    }
      break;
    case OLED_FILL_COLS:
    {
      // single column data fits into a simple i2c write
      i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(au8ColFillData), au8ColFillData);
      ++sVar.u8FilledColumns;
    }
      break;
    case OLED_FILL_FULL:
    {
      // full display fill (here: only 4 pages) requires 1K bytes (here 512 bytes)
      // i2c simple write cannot handle it.
      gsOledTxState.pu8Dat = gau8DataSeq2;
      gsOledTxState.u32DatLen = ARRAY_SIZE(gau8DataSeq2);
      gsOledTxState.u32Cur = 31;
      gsOledTxState.u8LastEnd = 0;
      i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, ARRAY_SIZE(gau8DataSeq2), (const uint8_t*) gau8DataSeq2);
    }
      break;
    case OLED_CMD_WRITE:
      if (sVar.u8CmdWriteCycle == WAITCYCLES) {
        sVar.u8CmdWriteCycle = 0;
      }
      if (sVar.u8CmdWriteCycle == 0) {
        if ((sVar.u32TotalDataWrites & 1) == 0) {
          // set GDRAM update window
          uint8_t u8PxXFirst = sCharsetAttr.u8Width * gu8CursorChrPosX;
          uint8_t u8PxXLast = u8PxXFirst + (sCharsetAttr.u8Width - 1);
          uint8_t u8ChrPerLine = (OLED_CHR_COLS / sCharsetAttr.u8Width);
          gu8OledTxBufferLen += ssd1306_set_hv_page_range(&gau8OledTxBuffer[gu8OledTxBufferLen], gu8CursorChrPosY, gu8CursorChrPosY);
          gu8OledTxBufferLen += ssd1306_set_hv_column_range(&gau8OledTxBuffer[gu8OledTxBufferLen], u8PxXFirst, u8PxXLast);
          // set data (to be written in next state)
          uint8_t u8CharIdx = (gu8CursorChrPosX + u8ChrPerLine * gu8CursorChrPosY) % 128; // here 128 is the length of the ASCII code table (by definition)
          u8CursorChrDataLen = 1 + ascii_get_tld(au8CursorChrData + 1, geCharset, u8CharIdx);
          // advance cursor
          ++gu8CursorChrPosX;
          if (OLED_CHR_COLS <= u8PxXLast + sCharsetAttr.u8Width) {
            gu8CursorChrPosX = 0;
            ++gu8CursorChrPosY;
            if (gu8CursorChrPosY == OLED_CHR_PAGES) {
              gu8CursorChrPosY = 0;
            }
          }
        }
      }
      if (1 < gu8OledTxBufferLen) {
        i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, gu8OledTxBufferLen, gau8OledTxBuffer);
        gu8OledTxBufferLen = 1;
      }
      ++sVar.u8CmdWriteCycle;
      break;
    case OLED_DATA_WRITE:
      for (int i = 1; i < u8CursorChrDataLen; ++i) {
        au8CursorChrData[i] = ~au8CursorChrData[i];
      }
      ssd1306_ctrl(au8CursorChrData, false, true);
      i2c_write(OLED_I2C_CH, OLED_I2C_SLAVEADDR, u8CursorChrDataLen, au8CursorChrData);
      ++sVar.u32TotalDataWrites;
      break;
  }
}

static void _oled_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = MS2TICKS(OLED_INIT_DELAY_MS);
  uint32_t u32NextLabel;

  if (u64tckNext <= u64tckNow) {
    if (lockmgr_acquire_lock(_i2c_to_lock(OLED_I2C_CH), &u32NextLabel)) {
      _oled_inner_cycle(u64tckNow, u32NextLabel);
      u64tckNext += MS2TICKS(OLED_PERIOD_MS);
    }
  }
}

// ISRs

IRAM_ATTR void _i2c_feed(void *pvParam) {
  FeedState *psParam = (FeedState*)pvParam;
  ++gu32FeedCnt;

  uint8_t u8IvalBegin = psParam->u8LastEnd;
  uint8_t u8IvalEnd = i2c_fifo_st_value(i2c_regs(OLED_I2C_CH), true, false);
  for (uint8_t u8Pos = u8IvalBegin; ((u8Pos & 0x1f) != u8IvalEnd) && psParam->u32Cur < psParam->u32DatLen; ++u8Pos) {
    i2c_nonfifo(OLED_I2C_CH)[u8Pos & 0x1f] = psParam->pu8Dat[psParam->u32Cur];
    ++psParam->u32Cur;
  }
  psParam->u8LastEnd = u8IvalEnd;
}

IRAM_ATTR void _i2c_start(void *pvParam) {
  uint64_t *pu64tckBegin = (uint64_t*)pvParam;
  *pu64tckBegin = timg_ticks(gsTimer);
}

IRAM_ATTR void _i2c_compl(void *pvParam) {
  gu32ComplFeed = gu32FeedCnt;
  uint64_t *pu64tckBegin = (uint64_t*)pvParam;
  uint64_t u64tckNow = timg_ticks(gsTimer);
  uart_printf(&gsUART0, "i2c ready %u\r\n", (uint32_t)TICKS2US(u64tckNow - *pu64tckBegin));
}

IRAM_ATTR void _i2c_error(void *pvParam) {
  ++gu32ErrorCnt;
  uart_printf(&gsUART0, "i2c error\r\n");
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);

  lockmgr_init();
  i2c_init_controller(OLED_I2C_CH, I2C0_SCL_GPIO, I2C0_SDA_GPIO, HZ2APBTICKS(OLED_I2C_FREQ_HZ));
  i2c_isr_init();

  i2c_isr_start(CPU_PRO, OLED_I2C_CH, I2C_INT_CH);

  i2c_isr_register(OLED_I2C_CH, I2C_INT_MASK_ERR, _i2c_error, NULL);

  _oled_init();
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
