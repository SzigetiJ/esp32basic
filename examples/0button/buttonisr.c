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
#include "iomux.h"
#include "main.h"
#include "defines.h"
#include "romfunctions.h"
#include "typeaux.h"
#include "uart.h"
#include "utils/uartutils.h"

// =================== Hard constants =================

#define BUTTON_GPIO       0U
#define INT_CH           22U

// ============= Local types ===============

// ================ Local function declarations =================
static void _button_init();
static void _button_cycle(uint64_t u64Ticks);
static void _button_isr(void *pvParam);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const char acMessage[] = "Button pressed.\n";

// ================ Local function definitions =================
IRAM_ATTR static void _button_isr(void *pvParam) {
  bool bButtonEvent = gsGPIO.STATUS & (1 << BUTTON_GPIO);
  if (bButtonEvent) {
    gsGPIO.STATUS_W1TC = (1 << BUTTON_GPIO);
    bool bLevel = gsGPIO.IN & (1 << BUTTON_GPIO);
    if (!bLevel) {
      for (int i = 0; i < ARRAY_SIZE(acMessage) - 1; ++i) {
        gsUART0.FIFO = acMessage[i];
      }
    }
  }
}

static void _button_init() {
  // setup iomux & gpio regs
  // note: GPIO0 is "preconfigured"
  IomuxGpioConfReg rIOMux;
  rIOMux.raw = iomux_get_gpioconf(BUTTON_GPIO);
  rIOMux.u1FunIE = 1;
  iomux_set_gpioconf(BUTTON_GPIO, rIOMux);
  SGpioPinReg sPinReg = {
    .u3PinIntType = 3,
    .u5PinIntEn = 5
  };
  gsGPIO.PIN[BUTTON_GPIO] = sPinReg;
  gsGPIO.STATUS_W1TC = (1 << BUTTON_GPIO);

  // register ISR and enable it
  ECpu eCpu = CPU_PRO;
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_GPIO_INTERRUPT_MAP : &dport_regs()->APP_GPIO_INTERRUPT_MAP);

  *prDportIntMap = INT_CH;
  _xtos_set_interrupt_handler_arg(INT_CH, _button_isr, 0);
  ets_isr_unmask(1 << INT_CH);
}

static void _button_cycle(uint64_t u64Ticks) {
  static uint64_t u64tckNext = 0;

  if (u64tckNext < u64Ticks) {
    bool bLevel = gsGPIO.IN & (1 << BUTTON_GPIO);
    gsUART0.FIFO = bLevel ? '^' : '_';
    u64tckNext += MS2TICKS(2000);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  gsUART0.CLKDIV.u20ClkDiv = APB_FREQ_HZ / 115200;
  _button_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _button_cycle(u64tckNow);
}
