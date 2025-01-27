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

#include "dht22.h"
#include "gpio.h"
#include "main.h"
#include "rmt.h"
#include "defines.h"
#include "romfunctions.h"
#include "iomux.h"
#include "dport.h"
#include "timg.h"
#include "utils/uartutils.h"

// =================== Hard constants =================
// #1: Timings
#define RMTDHT_PERIOD_MS  2000U     ///< Higher than 8 * 0.2s, so RMT blinks will not overlap.

// #2: Channels / wires / addresses
#define RMTDHT_GPIO         21U
#define RMTDHT_CH           RMT_CH0
#define RMTINT_CH           23U

// ============= Local types ===============

// ================ Local function declarations =================
static void _rmtdht_init();
static void _rmtdht_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static SDht22Descriptor gsDht22Desc;

// ==================== Implementation ================

void _done_rx(void *pvParam, SDht22Data *psParam) {
  uart_printf(&gsUART0, "INVALID: %02X %02X %02X %02X %02X\n",
          psParam->au8Invalid[0],
          psParam->au8Invalid[1],
          psParam->au8Invalid[2],
          psParam->au8Invalid[3],
          psParam->au8Invalid[4]
          );
  uart_printf(&gsUART0, "DATA: %02X %02X %02X %02X %02X\n",
          psParam->au8Data[0],
          psParam->au8Data[1],
          psParam->au8Data[2],
          psParam->au8Data[3],
          psParam->au8Data[4]
          );
  uart_printf(&gsUART0, "raw data (%c) T: %d, RH: %u\n",
          dht22_data_valid(psParam) ? '+' : '-',
          dht22_get_temp(psParam),
          dht22_get_rhum(psParam)
          );
}

static void _rmtdht_init() {
  rmt_isr_init();
  rmt_init_controller(true, true);
  gsDht22Desc = dht22_config(RMTDHT_CH, _done_rx, NULL);
  dht22_init(RMTDHT_GPIO, APB_FREQ_HZ, &gsDht22Desc);

  rmt_isr_start(CPU_PRO, RMTINT_CH);
}

static void _rmtdht_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;

  if (u64NextTick <= u64Ticks) {
    dht22_run(&gsDht22Desc);

    u64NextTick += MS2TICKS(RMTDHT_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  gsUART0.CLKDIV.u20ClkDiv = APB_FREQ_HZ / 115200;

  _rmtdht_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtdht_cycle(u64tckNow);
}
