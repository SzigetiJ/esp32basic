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
#include "typeaux.h"
#include "print.h"
#include "uart.h"

// =================== Hard constants =================
// #1: Timings
#define UART_FREQ_HZ        115200U
#define FLUSH_PERIOD_MS 2000U  ///< period of message flushing

// #2: buffer sizes
#define MSG_BUFSIZE 80U

// ============= Local types ===============

// ================ Local function declarations =================
static void _uart_init();
static void _print_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static UART_Type *gpsUART0 = &gsUART0;

// Implementation

static void _uart_init() {
  gpsUART0->CLKDIV = APB_FREQ_HZ / UART_FREQ_HZ;
}

static void _print_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static uint32_t u32Cnt = 0;
  static char acMsgPfx[] = "Hello World #";
  static char acMsgSfx[] = "!\r\n";

  if (u64NextTick <= u64Ticks) {
    char acBuf[MSG_BUFSIZE];
    char *pcBufE = acBuf;

    strcpy(pcBufE, acMsgPfx);
    pcBufE += ARRAY_SIZE(acMsgPfx) - 1;
    pcBufE = print_dec(pcBufE, ++u32Cnt);
    strcpy(pcBufE, acMsgSfx);
    pcBufE += ARRAY_SIZE(acMsgSfx) - 1;

    for (int i = 0; i < pcBufE - acBuf; ++i) {
      gpsUART0->FIFO = acBuf[i];
    }


    u64NextTick += MS2TICKS(FLUSH_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _uart_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _print_cycle(u64tckNow);
}
