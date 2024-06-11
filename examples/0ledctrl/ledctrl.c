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
#include "uart.h"

// =================== Hard constants =================
// #1: Timings
#define UART_FREQ_HZ        115200U
#define LEDCTRL_PERIOD_MS 100U

// #2: Channels / wires / addresses
#define LED1_GPIO 2U
#define LED2_GPIO 4U

// ============= Local types ===============

// ================ Local function declarations =================
static void _uart_init();
static void _led_init();
static void _ledctrl_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static UART_Type *gpsUART0 = &gsUART0;

// Implementation

static void _led_init() {
  gpio_pin_enable(LED1_GPIO);
  gpio_pin_enable(LED2_GPIO);
}

static void _uart_init() {
  gpsUART0->CLKDIV = APB_FREQ_HZ / UART_FREQ_HZ;
}

static void _ledctrl_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;

  if (u64NextTick <= u64Ticks) {
    while (0 < (gpsUART0->STATUS & 0xff)) {
      char cCtrl = gpsUART0->FIFO & 0xff;
      switch (cCtrl) {
        case 'y':
          gpio_reg_setbit(&gsGPIO.OUT_W1TS, LED1_GPIO);
          break;
        case 'Y':
          gpio_reg_setbit(&gsGPIO.OUT_W1TC, LED1_GPIO);
          break;
        case 'r':
          gpio_reg_setbit(&gsGPIO.OUT_W1TS, LED2_GPIO);
          break;
        case 'R':
          gpio_reg_setbit(&gsGPIO.OUT_W1TC, LED2_GPIO);
          break;
        default:
          gpsUART0->FIFO = '-';
      }
    }
    u64NextTick += MS2TICKS(LEDCTRL_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _led_init();
  _uart_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _ledctrl_cycle(u64tckNow);
}
