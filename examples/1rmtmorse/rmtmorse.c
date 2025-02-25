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
#include "typeaux.h"
#include "mphgen.h"
#include "utils/rmtutils.h"

// =================== Hard constants =================
// #0: Texts
#define MESSAGE "Hello, World!  "

// #1: Timings
#define MORSE_DIT_MS       200U
#define MORSE_DAH_MS       600U
#define MORSE_SSPACE_MS    300U    ///< Symbol space
#define MORSE_LSPACE_MS    600U    ///< Letter space
#define MORSE_WSPACE_MS   1200U    ///< Word space

#define UPDATE_PERIOD_MS   200U
#define RMT_DIVISOR         80U       ///< RMT TICK freq is 80MHz / RMT_DIVISOR (80 -> 1MHz, 40 -> 2MHz, 30 -> 2.66MHz)

#define CARRIER_EN           0U
#define CARRIER_HI_TCK   40000U
#define CARRIER_LO_TCK   40000U

// #2: Channels / wires / addresses
#define RMTMORSE_GPIO        2U
#define RMTMORSE_CH     RMT_CH0
//#define RMTINT_CH           23U // not used yet

// #3: Sizes
#define RMTMORSE_MEM_BLOCKS  1U
#define RMT_TXLIM        ((RMTMORSE_MEM_BLOCKS * RMT_RAM_BLOCK_SIZE)/2)
#define RMT_FEED0SIZE    (2 * RMT_TXLIM)

// ============= Local types ===============

// ================ Local function declarations =================

static uint16_t _mph_to_entry(EMorsePhase ePhase);
static uint16_t _mph2entry_next(void *pvState);
static bool _mph2entry_end(const void *pvState);

static void _rmt_config_channel(ERmtChannel eChannel, bool bLevel, bool bHoldLevel);
static void _rmtmorse_init();
static void _rmtmorse_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
const uint16_t gau16msPhaseLen[] = {
  MORSE_DIT_MS,
  MORSE_DAH_MS,
  MORSE_SSPACE_MS,
  MORSE_LSPACE_MS,
  MORSE_WSPACE_MS,
  0
};
const char acMessage[] = MESSAGE;

// ==================== Implementation ================

/**
 * Transforms a single morse phase into a single RMT (pre)entry using the timings
 * given in gau16msPhaseLen.
 * (Pre)entry means that the period of the entry is given in ms resolution,
 * thus it still needs to be stretched according to the RMT clock source and channel divisor.
 * @param ePhase Morse phase to transform.
 * @return RMT (pre)entry.
 */
static uint16_t _mph_to_entry(EMorsePhase ePhase) {
  uint16_t u16Ret = gau16msPhaseLen[ePhase];
  if (ePhase < MORSE_SSPACE) {
    u16Ret |= RMT_SIGNAL1;
  }
  return u16Ret;
}

/**
 * Generator function of the Morse phase to RMT entry generator.
 * @param pvState State descriptor of the underlying Morse phase generator.
 * @return Generator RMT (pre)entry.
 */
static uint16_t _mph2entry_next(void *pvState) {
  SMphGenState *psState = (SMphGenState*) pvState;
  EMorsePhase ePhase = mphgen_next(psState);
  return _mph_to_entry(ePhase);
}

/**
 * End of sequence indicator function of the Morse phase to RMT entry generator.
 * @param pvState State descriptor of the underlying Morse phase generator.
 * @return End of sequence.
 */
static bool _mph2entry_end(const void *pvState) {
  const SMphGenState *psState = (const SMphGenState*) pvState;
  return mphgen_end(psState);
}

static void _rmt_config_channel(ERmtChannel eChannel, bool bLevel, bool bHoldLevel) {
  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = RMT_DIVISOR, .u4MemSize = RMTMORSE_MEM_BLOCKS, .bCarrierEn = CARRIER_EN, .bCarrierOutLvl = 1},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1, .bIdleOutLvl = bLevel, .bIdleOutEn = bHoldLevel}
  };
  gpsRMT->asChConf[eChannel] = rChConf;

  if (CARRIER_EN) {
    SRmtChCarrierDutyReg rChCarr = {.u16High = CARRIER_HI_TCK, .u16Low = CARRIER_LO_TCK};
    gpsRMT->arCarrierDuty[eChannel] = rChCarr;
  }

  // set memory ownership of RMT RAM blocks
  SRmtChConf1Reg sRdMemCfg = {.raw = -1};
  sRdMemCfg.bMemOwner = 0;
  for (int i = 0; i < RMTMORSE_MEM_BLOCKS; ++i) {
    gpsRMT->asChConf[(eChannel + i) % RMT_CHANNEL_NUM].r1.raw &= sRdMemCfg.raw;
  }

  gpsRMT->arTxLim[eChannel].u9Val = RMT_TXLIM; // half of the memory block

  // Note: in this example we do not register ISRs
  // _rmtmorse_cycle() is responsible for checking and clearing RMT_INT status bits.
  gpsRMT->arInt[RMT_INT_ENA] =
          rmt_int_bit(eChannel, RMT_INT_TXEND) |
          rmt_int_bit(eChannel, RMT_INT_TXTHRES) |
          rmt_int_bit(eChannel, RMT_INT_ERR);
}

static void _rmtmorse_init() {
  rmt_init_controller(true, true);
  rmt_init_channel(RMTMORSE_CH, RMTMORSE_GPIO, false);
  _rmt_config_channel(RMTMORSE_CH, 0, 0);

  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV.u20ClkDiv = APB_FREQ_HZ / 115200;
}

static void _rmtmorse_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;

  static SByteGenState sByteGenState;
  static SMphGenState sMphGenState;
  static SStretchGenState sSGenState;
  static bool bFirstRun = true;
  static bool bFeedReady = false;
  static uint16_t u16RmtMemPos = 0;

  if (bFirstRun) {
    sByteGenState = bytegen_init((uint8_t*)acMessage, ARRAY_SIZE(acMessage) - 1);
    sMphGenState = mphgen_init(&sByteGenState, true);
    sSGenState = rmtutils_init_stretchgenstate(
            HZ2APBTICKS(1000) / RMT_DIVISOR, 1,
            _mph2entry_next, _mph2entry_end, &sMphGenState);

    bFeedReady = rmtutils_feed_tx_stretched(RMTMORSE_CH, &u16RmtMemPos, RMT_FEED0SIZE, (void*) &sSGenState);
    rmt_start_tx(RMTMORSE_CH, true);

    bFirstRun = false;
  }

  if (u64NextTick <= u64Ticks) {
    // dummy interrupt handling
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTMORSE_CH, RMT_INT_TXTHRES)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTMORSE_CH, RMT_INT_TXTHRES);
      if (!bFeedReady) {
        gsUART0.FIFO = 'F';
        bFeedReady = rmtutils_feed_tx_stretched(RMTMORSE_CH, &u16RmtMemPos, RMT_TXLIM, (void*) &sSGenState);
      }
    }
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTMORSE_CH, RMT_INT_TXEND)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTMORSE_CH, RMT_INT_TXEND);
      gsUART0.FIFO = '\n';
      mphgen_reset(&sMphGenState);
      u16RmtMemPos = 0U;
      bFeedReady = rmtutils_feed_tx_stretched(RMTMORSE_CH, &u16RmtMemPos, RMT_FEED0SIZE, (void*) &sSGenState);
      rmt_start_tx(RMTMORSE_CH, true);
    }
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTMORSE_CH, RMT_INT_ERR)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTMORSE_CH, RMT_INT_ERR);
      gsUART0.FIFO = 'R';
    }

    u64NextTick += MS2TICKS(UPDATE_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _rmtmorse_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtmorse_cycle(u64tckNow);
}
