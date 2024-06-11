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

#include "gpio.h"
#include "main.h"
#include "defines.h"

// =================== Hard constants =================
// #1: Timings
#define LEDBLINK_HPERIOD_MS 500U  ///< half period of blinking

// #2: Channels / wires / addresses
#define LEDBLINK_GPIO 2U

// ============= Local types ===============

// ================ Local function declarations =================
static void _ledblink_init();
static void _ledblink_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static volatile bool gbLedOn = false;

// Implementation

static void _ledblink_init() {
  gpio_pin_enable(LEDBLINK_GPIO);
}

static void _ledblink_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static RegAddr aprGpioOut[] = {&gsGPIO.OUT_W1TC, &gsGPIO.OUT_W1TS};

  if (u64NextTick <= u64Ticks) {
    gbLedOn = !gbLedOn;
    gpio_reg_setbit(aprGpioOut[gbLedOn], LEDBLINK_GPIO);
    u64NextTick += MS2TICKS(LEDBLINK_HPERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _ledblink_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _ledblink_cycle(u64tckNow);
}
