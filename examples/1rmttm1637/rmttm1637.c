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

// ============= Local types ===============

// ================ Local function declarations =================
static void _rmttm1637_ready(void *pvParam);
static void _rmttm1637_init();
static void _rmttm1637_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
const uint8_t gau8NumToSeg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};
static STm1637State gsTm1637State;
static const TimerId gsTimer = {.eTimg = TIMG_0, .eTimer = TIMER0};
static uint64_t gu64TckStart;
// ==================== Implementation ================

static void _rmttm1637_ready(void *pvParam) {
  STm1637State *psParam = (STm1637State*) pvParam;
  uint64_t u64TckStop = timg_ticks(gsTimer);
  uart_printf(&gsUART0, "Hello (%08X) %d ns\n", psParam->u32Internals, TICKS2NS((uint32_t) (u64TckStop - gu64TckStart)));
}

static void _rmttm1637_init() {
  rmt_isr_init();
  rmt_init_controller(true, true);
  STm1637Iface sIface = { CLK_GPIO, DIO_GPIO, CLK_CH, DIO_CH};
  gsTm1637State = tm1637_config(&sIface);
  tm1637_init(&gsTm1637State, APB_FREQ_HZ);
  tm1637_set_readycb(&gsTm1637State, _rmttm1637_ready, &gsTm1637State);
  gpio_pin_enable(2);
  gpio_pin_out_on(2);
  rmt_isr_start(CPU_PRO, RMTINT_CH);
}

static void _rmttm1637_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(RMTTM1637_PERIOD_MS);
  static uint8_t au8Data[4];
  static uint8_t u8DatIdx = 0;  // What number to display (put into au8Data)? Cycling through gau8NumToSeg: [0..15]
  static uint8_t u8Phase = 0;   // Display phase: last bit is 0: display dot (8th segment of the digits), bits 1, 2: brightness level

  if (u64NextTick <= u64Ticks) {
    // local, derived variables
    bool bDot = (0 == (u8Phase & 1));
    uint8_t u8Brightness = 7 - (3 * (u8Phase / 2)); // three levels: 7, 4, 1

    uart_printf(&gsUART0, "Cycle %u %u\t", u8DatIdx, u8Phase);
    tm1637_set_brightness(&gsTm1637State, true, u8Brightness);
    for (int i = 0; i < 4; ++i) {
      au8Data[i] = gau8NumToSeg[u8DatIdx] | (bDot ? 0x80 : 0x00);
    }
    tm1637_set_data(&gsTm1637State, au8Data);
    gu64TckStart = timg_ticks(gsTimer);
    tm1637_display(&gsTm1637State);

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
  gsUART0.CLKDIV = APB_FREQ_HZ / 115200;

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
