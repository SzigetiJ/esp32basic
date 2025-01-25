/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "esp_attr.h"
#include "esp32types.h"
#include "gpio.h"
#include "rmt.h"
#include "tm1637.h"

// Timings
/*
 * Timing table (with CLK_PERIOD_TICKS=6 the full data transmission length is 411 RMT cycles)
 * RMT_FREQ     TM1637_FREQ   Full transmission time
 * 1 MHz        166 KHz       411 µs
 * 2 MHz        333 KHz       205 µs
 * 2.5 MHz      416 KHz       164 µs
 * 3 MHz        500 KHz       137 µs
 * 3.2 MHz      533 KHz       129 µs
 * 4 MHz        666 KHz       103 µs
 */
#define RMT_FREQ_KHZ     3000U
#define CLK_PERIOD_TICKS 6U

#define CLK_HALFPERIOD_TICKS (CLK_PERIOD_TICKS / 2)
#define DIO_DELAY_TICKS 1U    ///< delay of DIO signal change to CLK falling edge

#define PREDIO_TICKS 2U       ///< wait time (rmt ticks) before DIO change in START/STOP
#define PRECLK_TICKS 2U       ///< wait time (rmt ticks) before command/data byte transfer: at the end of START/STOP/ACK

// Sizes and limits
#define SEQUENCE_SIZE 128

// ================= Internal Types ==================

typedef struct {
  uint16_t au16Dat[SEQUENCE_SIZE];
  uint8_t u8Idx;
} S_RMT_SEQUENCE;

// ================ Local function declarations =================
// Internal function declarations
static inline S_RMT_SEQUENCE *seq_reset_(S_RMT_SEQUENCE *psSeq);
static inline S_RMT_SEQUENCE *seq_append_(S_RMT_SEQUENCE *psSeq, uint16_t u16Value);
static void gen_endseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq);
static void gen_writebyteseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq, uint8_t u8Dat);
static void gen_startseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq);
static void gen_stopseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq);
static void _clktxend_isr(void *pvParam);
static void _reset_state(STm1637State *psState);
static void _next_byte(void *pvParam);

// =================== Global constants ================

// ==================== Local Data ================

// ==================== Implementation ================

/**
 * Simply resets the write cursor of the sequence.
 * @param psSeq
 * @return 
 */
static inline S_RMT_SEQUENCE *seq_reset_(S_RMT_SEQUENCE *psSeq) {
  psSeq->u8Idx = 0;
  return psSeq;
}

/**
 * Appends a single entry to the sequence.
 * @param psSeq
 * @param u16Value
 * @return 
 */
static inline S_RMT_SEQUENCE *seq_append_(S_RMT_SEQUENCE *psSeq, uint16_t u16Value) {
  psSeq->au16Dat[psSeq->u8Idx++] = u16Value;
  return psSeq;
}

/**
 * In RMT RAM the TX entry sequence must be terminated by a 0 long entry.
 * This function additionally guarranties that there will be even number of RMT RAM entries
 * in both Clk and Dio RMT RAM.
 * @param psClkSeq
 * @param psDioSeq
 */
static void gen_endseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq) {
  if (seq_append_(psClkSeq, 0)->u8Idx & 1) {
    seq_append_(psClkSeq, 0);
  }
  if (seq_append_(psDioSeq, 0)->u8Idx & 1) {
    seq_append_(psDioSeq, 0);
  }
}

/**
 * PreCond: CLK and DIO are synchronous (c:HI. d:?/LO)
 * At return CLK is at the end of the ACK bit,
 * whereas DIO is just behind the 8th bit (c:HI, d:??).
 * Internally DIO is delayed so that DIO level changes occur when CLK is LO.
 * CLK: {LLLHHH}*9, DIO: L{XXXXXX}*8.
 * @param psClkSeq
 * @param psDioSeq
 * @param u8Dat
 */
static void gen_writebyteseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq, uint8_t u8Dat) {
  seq_append_(psDioSeq, RMT_SIGNAL0 | (DIO_DELAY_TICKS));
  for (int i = 0; i < 9; i++) {
    seq_append_(psClkSeq, RMT_SIGNAL0 | (CLK_HALFPERIOD_TICKS));
    seq_append_(psClkSeq, RMT_SIGNAL1 | (CLK_HALFPERIOD_TICKS));

    if (i < 8) {
      seq_append_(psDioSeq, ((u8Dat & 0x01) ? RMT_SIGNAL1 : RMT_SIGNAL0) | (CLK_PERIOD_TICKS));
    }
    u8Dat >>= 1;
  }
}

/**
 * Precond: DIO and CLK are synchronous. (c:??, d:HI)
 * At return DIO is still synchronous (c:HI, d:LO).
 * CLK: HHHH, DIO: HHLL
 * @param psClkSeq
 * @param psDioSeq
 */
static void gen_startseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq) {

  seq_append_(psClkSeq, RMT_SIGNAL1 | (PREDIO_TICKS + PRECLK_TICKS));

  seq_append_(psDioSeq, RMT_SIGNAL1 | (PREDIO_TICKS));
  seq_append_(psDioSeq, RMT_SIGNAL0 | (PRECLK_TICKS));
}

/**
 * Precond: Directly after ACK (sync, c:HI, d:LO).
 * At return DIO and CLK are synchronous (and START may follow) (c:HI, d:HI)
 * CLK: LLLHHHH, DIO: LLLLHHH
 * @param psClkSeq
 * @param psDioSeq
 */
static void gen_stopseq(S_RMT_SEQUENCE *psClkSeq, S_RMT_SEQUENCE *psDioSeq) {

  seq_append_(psClkSeq, RMT_SIGNAL0 | (CLK_HALFPERIOD_TICKS));
  seq_append_(psClkSeq, RMT_SIGNAL1 | (PREDIO_TICKS + PRECLK_TICKS));

  seq_append_(psDioSeq, RMT_SIGNAL0 | (CLK_HALFPERIOD_TICKS + DIO_DELAY_TICKS));
  seq_append_(psDioSeq, RMT_SIGNAL1 | (PREDIO_TICKS + PRECLK_TICKS - DIO_DELAY_TICKS));
}

/**
 * There are two cases when CLK TXEND happens:
 * 1.) waiting for ACK (9th clk)
 * 2.) the whole communication process is done.
 * @param pvParam
 */
static void IRAM_ATTR _clktxend_isr(void *pvParam) {
  STm1637State *psParam = (STm1637State*) pvParam;

  if (TM1637_DATA_LEN < psParam->nDataI) {
    psParam->fReadyCb(psParam->pvReadyCbArg);
  } else {
    bool bDioHi = gpio_pin_read(psParam->sIface.u8DioPin);
    if (bDioHi) {
      psParam->u32Internals |= (1 << psParam->nDataI);
    }
    _next_byte(pvParam);
  }
}

/**
 * Constructs CLK and DIO RMT sequence.
 * Entry points: a.) Start of the whole communication process;
 * b.) ACK value is read from DIO when CLK is HI.
 * Exit points: a.) End of the whole communication process;
 * b.) ACK value must be read from DIO when CLK is HI.
 * @param pvParam TM1637 state descriptor.
 */
static void _next_byte(void *pvParam) {
  STm1637State *psParam = (STm1637State*) pvParam;
  S_RMT_SEQUENCE sClkSeq;
  S_RMT_SEQUENCE sDioSeq;

  bool bFirst = (psParam->nDataI == 0);     // first run CmdStart is not preceeded by CmdStop
  bool bDone = (psParam->nDataI == TM1637_DATA_LEN);
  bool bCmdStart = (psParam->nCmdIdxI < TM1637_CMDIDX_LEN && psParam->anCmdIdx[psParam->nCmdIdxI] == psParam->nDataI);
  bool bCmdStop = !bFirst && (bCmdStart || bDone);

  seq_reset_(&sClkSeq);
  seq_reset_(&sDioSeq);

  if (bCmdStop) {
    gen_stopseq(&sClkSeq, &sDioSeq);
  }
  if (bCmdStart) {
    gen_startseq(&sClkSeq, &sDioSeq);
    ++psParam->nCmdIdxI;
  }
  if (!bDone) {
    gen_writebyteseq(&sClkSeq, &sDioSeq, psParam->au8Data[psParam->nDataI]);
  }
  gen_endseq(&sClkSeq, &sDioSeq);

  memcpy((void*) rmt_ram_addr(psParam->sIface.eClkCh, 1, 0), (void*) sClkSeq.au16Dat, 2 * sClkSeq.u8Idx);
  memcpy((void*) rmt_ram_addr(psParam->sIface.eDioCh, 1, 0), (void*) sDioSeq.au16Dat, 2 * sDioSeq.u8Idx);

  ++psParam->nDataI;

  rmt_start_tx(psParam->sIface.eClkCh, true);
  rmt_start_tx(psParam->sIface.eDioCh, true);

}

static void _reset_state(STm1637State *psState) {
  psState->nDataI = 0;
  psState->nCmdIdxI = 0;
  psState->u32Internals = 0;
}

static void _rmt_config_channel(const STm1637Iface *psIface, uint8_t u8Divisor) {
  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = u8Divisor, .u4MemSize = 1,
      .bCarrierEn = false, .bCarrierOutLvl = 1},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1,
      .bIdleOutLvl = 1, .bIdleOutEn = 1, .bMemOwner = 0}
  };

  gpsRMT->asChConf[psIface->eClkCh] = rChConf;
  gpsRMT->asChConf[psIface->eDioCh] = rChConf;

  gpsRMT->arTxLim[psIface->eClkCh].u9Val = 256; // currently unused
  gpsRMT->arTxLim[psIface->eDioCh].u9Val = 256; // currently unused
}

// ============== Interface functions ==============

STm1637State tm1637_config(const STm1637Iface *psIface) {
  return (STm1637State){
    .sIface = *psIface,
    .au8Data =
    {0x40, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x88},
    .nDataI = 0,
    .anCmdIdx =
    {0, 1, 6},
    .nCmdIdxI = 0,
    .fReadyCb = NULL,
    .pvReadyCbArg = NULL};
}

void tm1637_init(STm1637State *psState, uint32_t u32ApbClkFreq) {
  rmt_init_channel(psState->sIface.eClkCh, psState->sIface.u8ClkPin, true);
  rmt_init_channel(psState->sIface.eDioCh, psState->sIface.u8DioPin, true);
  _rmt_config_channel(&psState->sIface, u32ApbClkFreq / (1000 * RMT_FREQ_KHZ));
  rmt_isr_register(psState->sIface.eClkCh, RMT_INT_TXEND, _clktxend_isr, psState);
}

/**
 * TODO
 * @param psState
 */
void tm1637_deinit(STm1637State *psState) {
  // remove interrupts

  // release GPIO-RMT bindings

  // conditionally power down RMT peripheral

  // release GPIO
}

void tm1637_set_data(STm1637State *psState, uint8_t *au8Data) {
  memcpy((psState->au8Data + 2), au8Data, 4);
}

void tm1637_set_brightness(STm1637State *psState, bool bOn, uint8_t u8Value) {
  psState->au8Data[6] = 0x80 | (bOn ? 0x08 : 0x00) | (u8Value & 0x07);
}

void tm1637_set_readycb(STm1637State *psState, Isr fHandler, void *pvArg) {
  psState->fReadyCb = fHandler;
  psState->pvReadyCbArg = pvArg;
}

void tm1637_display(STm1637State *psState) {
  _reset_state(psState);
  _next_byte(psState);
}
