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

#include "tm1637.h"
#include "gpio.h"
#include "main.h"
#include "rmt.h"
#include "defines.h"
#include "romfunctions.h"
#include "iomux.h"
#include "dport.h"
#include "timg.h"
#include "typeaux.h"
#include "utils/uartutils.h"

// =================== Hard constants =================
// #1: Timings
#define RMTTM1637_PERIOD_MS  500U

// #2: Channels / wires / addresses
#define CLK_GPIO         21U
#define DIO_GPIO         19U
#define CLK_CH           RMT_CH1
#define DIO_CH           RMT_CH0

#define RMTINT_CH           23U

// #3 Sizes
#define TM1637_CELLS      4U
#define TM1637_COLON_POS  1U

// ============= Local types ===============

typedef struct {
  STm1637State *psState;
  uint64_t u64tckStart;
} SReadyCbParam;

// ================ Local function declarations =================
static void _rmttm1637_ready(void *pvParam);
static void _rmttm1637_init();
static void _rmttm1637_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const TimerId gsTimer = {.eTimg = TIMG_0, .eTimer = TIMER0};
const uint8_t gau8NumToSeg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};

static uint8_t gau8Tm1637Data[TM1637_CELLS];
static STm1637State gsTm1637State;
static SReadyCbParam gsReadyData;

// ==================== Implementation ================

static void _rmttm1637_ready(void *pvParam) {
  SReadyCbParam *psParam = (SReadyCbParam*) pvParam;
  uint64_t u64TckStop = timg_ticks(gsTimer);
  uart_printf(&gsUART0, "Display ready (failed ACKs: %03X)\tDt: %d ns\n",
          psParam->psState->abNak & 0xfff,
          TICKS2NS((uint32_t) (u64TckStop - psParam->u64tckStart)));
}

static void _rmttm1637_init() {
  rmt_isr_init();
  rmt_init_controller(true, true);
  STm1637Iface sIface = { CLK_GPIO, DIO_GPIO, CLK_CH, DIO_CH};
  gsTm1637State = tm1637_config(&sIface, gau8Tm1637Data);
  tm1637_init(&gsTm1637State, APB_FREQ_HZ);
  tm1637_set_readycb(&gsTm1637State, _rmttm1637_ready, &gsReadyData);
  tm1637_set_brightness(&gsTm1637State, true, 7);
  gsReadyData.psState = &gsTm1637State;
  rmt_isr_start(CPU_PRO, RMTINT_CH);
}

static void _rmttm1637_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(RMTTM1637_PERIOD_MS);
  static uint8_t u8DatIdx = 0;  // What number to display (put into au8Data)? Cycling through gau8NumToSeg: [0..15]
  static uint8_t u8Phase = 0;   // Display phase

  if (u64NextTick <= u64Ticks) {
    uart_printf(&gsUART0, "Cycle %u %u\t", u8DatIdx, u8Phase);
    switch (u8Phase) {
      case 0: // display new data with colon
        for (int i = 0; i < TM1637_CELLS; ++i) {
          gau8Tm1637Data[i] = gau8NumToSeg[u8DatIdx];
          gau8Tm1637Data[TM1637_COLON_POS] |= 0x80;
        }
        gsReadyData.u64tckStart = timg_ticks(gsTimer);
        tm1637_flush_full(&gsTm1637State, TM1637_CELLS);
        break;
      case 1: // remove colon/dot
        gau8Tm1637Data[TM1637_COLON_POS] &= 0x7f;
        gsReadyData.u64tckStart = timg_ticks(gsTimer);
        tm1637_flush_range(&gsTm1637State, TM1637_COLON_POS, 1);
        break;
      case 2: // low brightness
      case 3: // high brightness
        tm1637_set_brightness(&gsTm1637State, true, u8Phase == 2 ? 2 : 7);
        gsReadyData.u64tckStart = timg_ticks(gsTimer);
        tm1637_flush_brightness(&gsTm1637State);
        break;
      case 4: // display '-' after colon
        for (int i = TM1637_COLON_POS + 1; i < TM1637_CELLS; ++i) {
          gau8Tm1637Data[i] = 0x40;
        }
        gau8Tm1637Data[TM1637_COLON_POS] |= 0x80;
        gsReadyData.u64tckStart = timg_ticks(gsTimer);
        tm1637_flush_range(&gsTm1637State, TM1637_COLON_POS, TM1637_CELLS - TM1637_COLON_POS);
        break;
      default:  // remove colon/dot
        gau8Tm1637Data[TM1637_COLON_POS] &= 0x7f;
        gsReadyData.u64tckStart = timg_ticks(gsTimer);
        tm1637_flush_range(&gsTm1637State, TM1637_COLON_POS, 1);
    }
    // jump to next state
    ++u8Phase;
    if (6 == u8Phase) {
      u8Phase = 0;
      ++u8DatIdx;
      if (u8DatIdx == ARRAY_SIZE(gau8NumToSeg)) {
        u8DatIdx = 0;
      }
    }
    u64NextTick += MS2TICKS(RMTTM1637_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  gsUART0.CLKDIV.u20ClkDiv = APB_FREQ_HZ / 115200;
  gsUART0.CLKDIV.u4ClkDivFrag = 7;
  _rmttm1637_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmttm1637_cycle(u64tckNow);
}
