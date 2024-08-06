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
#define BLINKLEN_MS         200U      ///< Length of a single period (on + off part).
#define RMTBLINK_PERIOD_MS  2000U     ///< Higher than 8 * 0.2s, so RMT blinks will not overlap.
#define RMT_DIVISOR         80U       ///< RMT TICK freq is 80MHz / RMT_DIVISOR (80 -> 1MHz, 40 -> 2MHz, 30 -> 2.66MHz)

// #2: Channels / wires / addresses
#define RMTBLINK_GPIO       2U
#define RMTBLINK_CH         RMT_CH0

// #3: Sizes
#define RMT_AUXBUF_SIZE     128U

// ============= Local types ===============

// ================ Local function declarations =================
static void _rmt_init_controller();
static void _rmt_init_channel(E_RMT_CHANNEL eChannel, uint8_t u8Pin, bool bLevel, bool bHoldLevel);
static void _rmtblink_init();
static void _rmtblink_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
// The on / off length of the blinking is defined as the product of ~Base and ~Mul.
// Why? Because the on/off lengths are measured in APB clock / divisor ticks (1MHz),
// and the chosen values (0.1s = 100000 ticks) are higher than the RMT RAM entries can hold as period values.
// Thus multiple concatenated RMT entries will define a single on or off period.
// ~Base cannot exceed 32767 (limit of RMT entry.period).
// ~Mul * BlinkMax must be less than 128 (RMT RAM (by default) can contain 256 entries,
// and we use at most 2 * BlinkMax * BlinkLenMul).
static const uint16_t gu16usBlinkLenBase = 25000U;
static const uint16_t gu8BlinkLenMul = (BLINKLEN_MS * 1000) / (2 * gu16usBlinkLenBase);   // BLINKLEN=200 => 4.
static const uint8_t gu8BlinkMax = 8;

// ==================== Implementation ================

static void _rmt_init_controller() {
  dport_regs()->PERIP_CLK_EN |= 1 << DPORT_PERIP_BIT_RMT;

  dport_regs()->PERIP_RST_EN |= 1 << DPORT_PERIP_BIT_RMT;
  dport_regs()->PERIP_RST_EN &= ~(1 << DPORT_PERIP_BIT_RMT);

  S_RMT_APB_CONF_REG rApbConf = {.bMemAccessEn = 1, .bMemTxWrapEn = 1}; // direct RMT RAM access (not using FIFO), mem wrap-around (not used)
  gpsRMT->rApb = rApbConf;
}

static void _rmt_init_channel(E_RMT_CHANNEL eChannel, uint8_t u8Pin, bool bLevel, bool bHoldLevel) {
  // gpio & iomux
  bLevel ? gpio_pin_out_on(u8Pin) : gpio_pin_out_off(u8Pin); // set GPIO level when not bound to RMT (optional)

  IomuxGpioConfReg rRmtConf = {.u1FunIE = 1, .u1FunWPU = 1, .u3McuSel = 2}; // input enable, pull-up, iomux function
  iomux_set_gpioconf(u8Pin, rRmtConf);

  gpio_pin_enable(u8Pin);
  gpio_matrix_out(u8Pin, rmt_out_signal(eChannel), 0, 0);
  gpio_matrix_in(u8Pin, rmt_in_signal(eChannel), 0);

  // rmt channel config
  S_RMT_CHCONF rChConf = {
    .r0 =
    {.u8DivCnt = RMT_DIVISOR, .u4MemSize = 1},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1, .bIdleOutLvl = bLevel, .bIdleOutEn = bHoldLevel}
  };
  gpsRMT->asChConf[eChannel] = rChConf;

  gpsRMT->arTxLim[eChannel].u9Val = 256; // currently unused
}

static void _rmtblink_init() {
  _rmt_init_controller();
  _rmt_init_channel(RMTBLINK_CH, RMTBLINK_GPIO, 0, 0);
}

static void _rmtblink_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;
  static uint16_t au16BlinkPattern[RMT_AUXBUF_SIZE]; // aux. buffer
  static uint8_t u8Blink = 1;

  if (u64NextTick <= u64Ticks) {
    // set pattern
    memset(au16BlinkPattern, 0, 2 * RMT_AUXBUF_SIZE);
    for (int i = 0; i < u8Blink; ++i) {
      for (int j = 0; j < gu8BlinkLenMul; ++j) {
        au16BlinkPattern[2 * i * gu8BlinkLenMul + j] = RMT_SIGNAL1 | gu16usBlinkLenBase;
        au16BlinkPattern[(2 * i + 1) * gu8BlinkLenMul + j] = RMT_SIGNAL0 | gu16usBlinkLenBase;
      }
    }
    if (u8Blink == gu8BlinkMax) {
      u8Blink = 0;
    }
    ++u8Blink;

    // write pattern into rmt ram & start tx
    memcpy((void*) rmt_ram_block(RMTBLINK_CH), au16BlinkPattern, 2 * RMT_AUXBUF_SIZE);
    gpsRMT->arInt[RMT_INT_ENA] = rmt_int_bit(RMTBLINK_CH, RMT_INT_TXEND);
    rmt_start_tx(RMTBLINK_CH, 1);

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
