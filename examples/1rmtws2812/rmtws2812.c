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
#include "rmt.h"
#include "defines.h"
#include "romfunctions.h"
#include "uart.h"
#include "utils/uartutils.h"
#include "iomux.h"
#include "dport.h"
#include "typeaux.h"
#include "esp_attr.h"
#include "ws2812.h"

// =================== Hard constants =================

// #1: Timings -- 50ms: 20Hz update freq.
#define UPDATE_PERIOD_MS      50U   ///< WS2812B update period
#define BUF_UPDATE_PERIOD_MS  50U   ///< Data update period

// #2: Channels / wires / addresses
#define RMTWS2812_GPIO      21U
#define RMTWS2812_CH    RMT_CH0
#define RMTINT_CH           23U

// #3: Sizes and iteration cycles
#define STRIP_LENGTH          12U
#define STRIP_FRONT_LEN        2U
#define STRIP_BACK_LEN         2U
#define STRIP_INTERPOLATION_STEPS   5U
#define STRIP_GRADCHANGE_STEPS     30U
#define STRIP_ROTATE_CYCLES         3U

// #4: RMT buffer settings
#define RMTWS2812_MEM_BLOCKS  1U

// #5: colors
#define G_MAX 0x7F
#define G_MIX 0x5F
#define R_MAX 0xFF
#define R_MIX 0x7F
#define B_MAX 0xFF
#define B_MIX 0xBF

// ============= Local types ===============

typedef uint8_t Color[3];

// ================ Local function declarations =================

static void _fill_prebuffer(uint8_t *pu8Dest, uint8_t u8Stop0Idx, uint8_t u8Stop1Idx);
static uint8_t _weighted_avg(uint8_t u8A, uint8_t u8B, uint32_t u32WeightA, uint32_t u32WeightB);
static uint8_t *_buf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint8_t *pu8B, uint32_t u32WeightA, uint32_t u32WeightB);
static uint8_t *_rotbuf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint32_t u32AOffset, uint8_t *pu8B, uint32_t u32BOffset, uint32_t u32WeightA, uint32_t u32WeightB);
static void _rmtws2812_init_data();
static void _rmtws2812_init_peripheral();
static void _rmtws2812_cycle(uint64_t u64Ticks);
static void _buf_update_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

const Color gasStops[] = {
  {0, R_MAX, 0},        // R
  {G_MIX, R_MIX, 0},    // Y
  {0, 0, B_MAX},        // B
  {G_MAX, 0, 0},        // G
  {0, R_MIX, B_MIX},    // M
  {0, 0, 0},            // Off
  {G_MIX, 0, B_MIX},    // C
  {G_MIX, R_MIX, B_MIX} // W
};

// ==================== Local Data ================

static uint8_t gau8PreBuffer0[3 * STRIP_LENGTH];
static uint8_t gau8PreBuffer1[3 * STRIP_LENGTH];
static uint8_t gau8Buffer[3 * STRIP_LENGTH];
static const SWs2812Iface gsWs2812Iface = {.eChannel = RMTWS2812_CH, .u8Blocks = RMTWS2812_MEM_BLOCKS};
static SWs2812FeederState gsFeederState = {.sIface = gsWs2812Iface, .pu8Data = gau8Buffer, .szLen = sizeof (gau8Buffer), .szPos = 0};

// ==================== Implementation ================
static void _fill_prebuffer(uint8_t *pu8Dest, uint8_t u8Stop0Idx, uint8_t u8Stop1Idx) {
  for (int i = 0; i < STRIP_FRONT_LEN; ++i) {
    memcpy(pu8Dest + (3 * i), gasStops[u8Stop0Idx], 3);
  }
  for (int i = STRIP_FRONT_LEN; i < (STRIP_LENGTH - STRIP_BACK_LEN); ++i) {
    int div = STRIP_LENGTH - STRIP_FRONT_LEN - STRIP_BACK_LEN + 1;
    int j = i - STRIP_FRONT_LEN + 1;  // jmax=(imax=STRIP_LENGTH - STRIP_BACK_LEN - 1) - STRIP_FRONT_LEN + 1 = div - 1
    int k = div - j;
    pu8Dest[3 * i] = (k * gasStops[u8Stop0Idx][0] + j * gasStops[u8Stop1Idx][0]) / div;
    pu8Dest[3 * i + 1] = (k * gasStops[u8Stop0Idx][1] + j * gasStops[u8Stop1Idx][1]) / div;
    pu8Dest[3 * i + 2] = (k * gasStops[u8Stop0Idx][2] + j * gasStops[u8Stop1Idx][2]) / div;
  }
  for (int i = STRIP_LENGTH - STRIP_BACK_LEN; i < STRIP_LENGTH; ++i) {
    memcpy(pu8Dest + (3 * i), gasStops[u8Stop1Idx], 3);
  }
}

static uint8_t _weighted_avg(uint8_t u8A, uint8_t u8B, uint32_t u32WeightA, uint32_t u32WeightB) {
  return (u32WeightA * u8A + u32WeightB * u8B) / (u32WeightA + u32WeightB);
}

static uint8_t *_buf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint8_t *pu8B, uint32_t u32WeightA, uint32_t u32WeightB) {
  for (uint32_t i = 0; i < u32Len; ++i) {
    pu8Res[i] = _weighted_avg(pu8A[i], pu8B[i], u32WeightA, u32WeightB);
  }
  return pu8Res;
}

static uint8_t *_rotbuf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint32_t u32AOffset, uint8_t *pu8B, uint32_t u32BOffset, uint32_t u32WeightA, uint32_t u32WeightB) {
  for (uint32_t i = 0; i < u32Len; ++i) {
    pu8Res[i] = _weighted_avg(pu8A[(u32AOffset + i) % u32Len], pu8B[(u32BOffset + i) % u32Len], u32WeightA, u32WeightB);
  }
  return pu8Res;
}

IRAM_ATTR static void _rmtws2812_txend_cb(void *pvParam) {
  SWs2812FeederState *psParam = (SWs2812FeederState*)pvParam;
  psParam->bBusy = false;
}

static void _rmtws2812_init_data() {
  _fill_prebuffer(gau8PreBuffer0, 0, 1);
  _buf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), gau8PreBuffer0, gau8PreBuffer0, 1, 0);
}

static void _rmtws2812_init_peripheral() {
  rmt_isr_init(); // ws2812_init will write into rmt_isr table
  rmt_init_controller(true, true);
  ws2812_init(RMTWS2812_GPIO, APB_FREQ_HZ, _rmtws2812_txend_cb, &gsFeederState);

  // enable RMT ISR
  rmt_isr_start(CPU_PRO, RMTINT_CH);
}

static void _rmtws2812_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static bool bFirstRun = true;

  if (bFirstRun) {
    ws2812_start(&gsFeederState);
    bFirstRun = false;
  }

  if (u64NextTick <= u64Ticks) {
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND);
      gsUART0.FIFO = 'E';
    }
    if (gsFeederState.szPos == gsFeederState.szLen && !gsFeederState.bBusy) {
      gsFeederState.szPos = 0;
      ws2812_start(&gsFeederState);
    }
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR);
      gsUART0.FIFO = 'R';
    }
    u64NextTick += MS2TICKS(UPDATE_PERIOD_MS);
  }
}

static void _buf_update_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(1025);
  static uint32_t u32Shift = 0;
  static uint32_t u32SubShift = 0;
  static uint8_t u8RotCycleCnt = 0;
  static bool bRotateBuffer = true; // true: cycle within buffer with shift and subshift, false: replace gradient
  static bool bUsePreBuf0 = true;   // which buffer to use (PreBuf0 vs. PreBuf1)
  static uint8_t u8Stop0Idx = 0;    // Color index on gasStops, identifies the color at the buffer begin
  static uint8_t u8Stop1Idx = 1;    // Color index on gasStops, identifies the color at the buffer end
  static bool bStopSwap = false;

  if (u64NextTick <= u64Ticks) {
    uint8_t *pu8PreBuf = bUsePreBuf0 ? gau8PreBuffer0 : gau8PreBuffer1;   // current main prebuffer
    uint8_t *pu8XPreBuf = !bUsePreBuf0 ? gau8PreBuffer0 : gau8PreBuffer1; // other prebuffer
    if (bRotateBuffer) {
      ++u32SubShift;
      if (u32SubShift == STRIP_INTERPOLATION_STEPS) {
        u32SubShift = 0;
        ++u32Shift;
        if (u32Shift == STRIP_LENGTH) {
          u32Shift = 0;
        }
      }
      _rotbuf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), pu8PreBuf, 3 * u32Shift, pu8PreBuf, 3 * (u32Shift + 1), STRIP_INTERPOLATION_STEPS - u32SubShift, u32SubShift);
      if (u32Shift == 0 && u32SubShift == 0) {  // end of buffer rotation
        ++u8RotCycleCnt;
        if (u8RotCycleCnt == STRIP_ROTATE_CYCLES) {
          u8RotCycleCnt = 0;
          bStopSwap = !bStopSwap;
          uint8_t tmp = u8Stop1Idx;
          u8Stop1Idx = u8Stop0Idx;
          u8Stop0Idx = tmp;
          if (!bStopSwap) {
            ++u8Stop0Idx;
            ++u8Stop1Idx;
            if (u8Stop0Idx == ARRAY_SIZE(gasStops)) {
              u8Stop0Idx = 0;
              ++u8Stop1Idx;
            }
            u8Stop1Idx %= ARRAY_SIZE(gasStops);
            if (u8Stop1Idx == u8Stop0Idx) {
              ++u8Stop1Idx;
            }
          }
          uart_printf(&gsUART0, "Changing gradient to Color#%u -> Color#%u\n", u8Stop0Idx, u8Stop1Idx);
          _fill_prebuffer(pu8XPreBuf, u8Stop0Idx, u8Stop1Idx);
          bRotateBuffer = false;
        }
      }
    } else {  // not rotating buffer
      ++u32SubShift;
      _buf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), pu8PreBuf, pu8XPreBuf, STRIP_GRADCHANGE_STEPS - u32SubShift, u32SubShift);
      if (u32SubShift == STRIP_GRADCHANGE_STEPS) {  // end of gradient replacement
        u32SubShift = 0;
        bRotateBuffer = true;
        bUsePreBuf0 = !bUsePreBuf0;
      }
    }
    u64NextTick += MS2TICKS(BUF_UPDATE_PERIOD_MS);
  }
}


// ====================== Interface functions =========================

void prog_init_pro_pre() {
  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV = APB_FREQ_HZ / 115200;

  _rmtws2812_init_data();
  _rmtws2812_init_peripheral();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtws2812_cycle(u64tckNow);
  _buf_update_cycle(u64tckNow);
}
