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
#include "rmt.h"
#include "defines.h"
#include "romfunctions.h"
#include "print.h"
#include "uart.h"
#include "iomux.h"
#include "dport.h"

// =================== Hard constants =================
// #1: Timings
#define RMTBLINK_PERIOD_MS  2000U
#define RMT_DIVISOR         80U       ///< RMT TICK freq is 80MHz / RMT_DIVISOR (80 -> 1MHz, 40 -> 2MHz, 30 -> 2.66MHz)

// #2: Channels / wires / addresses
#define RMTBLINK_GPIO       2U
#define RMTBLINK_CH         RMT_CH0

// #3: Sizes
#define RMT_AUXBUF_SIZE     128U

// ============= Local types ===============

// ================ Local function declarations =================
static void _rmt_init_channel(E_RMT_CHANNEL eChannel, uint8_t u8Pin);
static void _rmtblink_init();
static void _rmtblink_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
// the on / off length of the blinking is defined as the product of ~Base and ~Mul.
// ~Base cannot exceed 32767.
// ~Mul must be less than 16.
const uint16_t u16usBlinkLenBase = 25000U;
const uint16_t u8BlinkLenMul = 4;

// ==================== Implementation ================

static void _rmt_init_channel(E_RMT_CHANNEL eChannel, uint8_t u8Pin) {
  gpio_pin_out_on(u8Pin);
  IomuxGpioConfReg rRmtConf = {.u1FunIE = 1, .u1FunWPU = 1, .u3McuSel = 2};
  iomux_set_gpioconf(u8Pin, rRmtConf);

  // output enable
  gpio_pin_enable(u8Pin);
  gpio_matrix_out(u8Pin, rmt_out_signal(eChannel), 0, 0);
  //  gpio_matrix_in(u8Pin, rmt_in_signal(eChannel), 0);

  dport_regs()->PERIP_CLK_EN |= 1 << DPORT_PERIP_BIT_RMT;

  dport_regs()->PERIP_RST_EN |= 1 << DPORT_PERIP_BIT_RMT;
  dport_regs()->PERIP_RST_EN &= ~(1 << DPORT_PERIP_BIT_RMT);

  S_RMT_CHCONF0_REG rChConf0 = {.u8DivCnt = RMT_DIVISOR, .u4MemSize = 1};
  S_RMT_CHCONF1_REG rChConf1 = {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1};
  S_RMT_APB_CONF_REG rApbConf = {.bMemAccessEn = 1, .bMemTxWrapEn = 1};

  gpsRMT->asChConf[RMTBLINK_CH].r0 = rChConf0;
  gpsRMT->asChConf[RMTBLINK_CH].r1 = rChConf1;
  gpsRMT->rApb = rApbConf;
  gpsRMT->arTxLim[RMTBLINK_CH].u9Val = 256; // currently unused
}

static void _rmtblink_init() {
  _rmt_init_channel(RMTBLINK_CH, RMTBLINK_GPIO);
}

static void _rmtblink_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static uint16_t au16BlinkPattern[RMT_AUXBUF_SIZE]; // aux. buffer
  static uint8_t u8Blink = 1;
  const uint8_t u8BlinkMax = 8;

  if (u64NextTick <= u64Ticks) {
    // reset mem(ptr)
    S_RMT_CHCONF1_REG rConf1RdRst = {.bMemRdRst = 1};
    S_RMT_CHCONF1_REG rConf1TxSt = {.bTxStart = 1};
    register_set_bits((RegAddr)&(gpsRMT->asChConf[RMTBLINK_CH].r1), rConf1RdRst.raw, rConf1RdRst.raw);

    // set pattern
    memset(au16BlinkPattern, 0, 2 * RMT_AUXBUF_SIZE);
    for (int i = 0; i < u8Blink; ++i) {
      for (int j = 0; j < u8BlinkLenMul; ++j) {
        au16BlinkPattern[2 * i * u8BlinkLenMul + j] = RMT_SIGNAL1 | u16usBlinkLenBase;
        au16BlinkPattern[(2 * i + 1) * u8BlinkLenMul + j] = RMT_SIGNAL0 | u16usBlinkLenBase;
      }
    }
    if (u8Blink == u8BlinkMax) {
      u8Blink = 0;
    }
    ++u8Blink;
    // write pattern into rmt ram
    memcpy((void*) rmt_ram_block(RMTBLINK_CH), au16BlinkPattern, 2 * RMT_AUXBUF_SIZE);

    // start tx
    register_set_bits((RegAddr)&(gpsRMT->asChConf[RMTBLINK_CH].r1), rConf1TxSt.raw, rConf1TxSt.raw);

    u64NextTick += MS2TICKS(RMTBLINK_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _rmtblink_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtblink_cycle(u64tckNow);
}
