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

#define RMT_ENTRYPAIR(LVL0, LEN0, LVL1, LEN1) ((((LVL0) ? RMT_SIGNAL1 : RMT_SIGNAL0) | (LEN0)) | ((((LVL1) ? RMT_SIGNAL1 : RMT_SIGNAL0) | (LEN1)) << 16))

// Timings
/*
 * Timing table
 * The full data transmission length (dt1: 3 commands, 4 digits)
 * is 7*9*CLK_PERIOD_TICKS + 3*2*CLK_PERIOD_TICKS = 69*CLK_PERIOD_TICKS.
 * Short data transmission length (dt2: 1 command, 1 digit)
 * is 2*9*CLK_PERIOD_TICKS + 1*2*CLK_PERIOD_TICKS = 20*CLK_PERIOD_TICKS.
 * A: with CLK_PERIOD_TICKS=6 the full length is 414 RMT cycles
 * B: with CLK_PERIOD_TICKS=8 the full length is 552 RMT cycles
 * RMT_FREQ     TM1637_FREQ(A)  dt1(A)  dt2(A)    TM1637_FREQ(B)  dt1(B)  dt2(B)
 * 1 MHz        166 KHz         414 µs  120 µs    125 KHz         552 µs  160 µs
 * 2 MHz        333 KHz         207 µs  60 µs     250 KHz         276 µs  80 µs
 * 2.5 MHz      416 KHz         166 µs  48 µs     312.5 KHz       221 µs  64 µs
 * 3 MHz        500 KHz         138 µs  40 µs     375 KHz         184 µs  53 µs
 * 3.2 MHz      533 KHz         129 µs  37 µs     400 KHz         172 µs  50 µs
 * 4 MHz        666 KHz         103 µs  30 µs     500 KHz         138 µs  40 µs
 * 5 MHz        833 KHz         828 µs  24 µs     625 KHz         110 µs  32 µs
 */
#define RMT_FREQ_KHZ     3000U
#define CLK_PERIOD_TICKS 6U

#define CLK_HALFPERIOD_TICKS (CLK_PERIOD_TICKS / 2)
#define DIO_DELAY_TICKS 0U    ///< delay of DIO signal change to CLK falling edge

// Commands
#define CMD_SETDATA     0x40
#define CMD_SETADDRESS  0xC0
#define CMD_CTRLDISPLAY 0x80

// ================= Internal Types ==================

typedef struct {
  uint8_t u4DatE;
  uint8_t u4CmdE;
} SInternals;

// ================ Local function declarations =================
static inline void _init_clkseq(ERmtChannel eChannel);
static inline void _update_clkseq(ERmtChannel eChannel, bool bStop, bool bStart);
static inline void _dat_dioseq(RegAddr prDioRam, uint8_t u8Dat);
static inline void _update_dioseq(ERmtChannel eChannel, bool bStop, bool bStart, uint8_t u8Dat);
static void _clktxend_isr(void *pvParam);
static void _diotxend_isr(void *pvParam);
static void _reset_state(STm1637State *psState, SInternals sX);
static void _next_byte(void *pvParam);
static void _start_tx_process(STm1637State *psState, SInternals sX);

// =================== Global constants ================

// ==================== Local Data ================

// ==================== Implementation ================
/**
 * When the DIO TX process is over, CLK TX is still running.
 * Here we have CLK_PERIOD_TICKS - DIO_DELAY_TICKS to do
 * some time consuming calculations.
 * Note, here we update the RMT RAM of the CLK channel, however, as we modify olny its first 2 registers,
 * the update does not disturb the ongoing CLK TX process.
 * @param pvParam TM1637 state descriptor.
 */
static void IRAM_ATTR _diotxend_isr(void *pvParam) {
  STm1637State *psParam = (STm1637State*) pvParam;

  bool bFirst = (psParam->sByteI.u8Cur == psParam->sByteI.u8Begin);     // first run CmdStart is not preceeded by CmdStop
  bool bDone = (psParam->sByteI.u8Cur == psParam->sByteI.u8End);
  bool bCmdStart = (psParam->sCmdIdxI.u8Cur < psParam->sCmdIdxI.u8End && psParam->au8CmdIdx[psParam->sCmdIdxI.u8Cur] == psParam->sByteI.u8Cur);
  bool bCmdStop = !bFirst && (bCmdStart || bDone);

  _update_clkseq(psParam->sIface.eClkCh, bCmdStop, bCmdStart);
  _update_dioseq(psParam->sIface.eDioCh, bCmdStop, bCmdStart, bDone ? 0 : psParam->au8Bytes[psParam->sByteI.u8Cur]);
  if (bCmdStart) {
    ++psParam->sCmdIdxI.u8Cur;
  }
}

/**
 * There are two cases when CLK TXEND happens:
 *  1. waiting for ACK (9th clk);
 *  2. the whole communication process is done.
 * If the whole communication process is not over,
 * the transmission of the next byte (_next_byte()) is called by this method.
 * @param pvParam TM1637 state descriptor.
 */
static void IRAM_ATTR _clktxend_isr(void *pvParam) {
  STm1637State *psParam = (STm1637State*) pvParam;

  if (psParam->sByteI.u8End < psParam->sByteI.u8Cur) {
    psParam->fReadyCb(psParam->pvReadyCbArg);
  } else {
    bool bDioHi = gpio_pin_read(psParam->sIface.u8DioPin);
    if (bDioHi) {
      psParam->abNak |= (1 << (psParam->sByteI.u8Cur - psParam->sByteI.u8Begin));
    }
    _next_byte(pvParam);
  }
}

/**
 * Initializes the RMT RAM of the CLK process.
 * Note, this RMT RAM entry sequence is slightly modified before every byte transmission,
 * as STOP / START slices may be added (prepended) to the sequence.
 * @param eChannel CLK RMT channel.
 */
static inline void _init_clkseq(ERmtChannel eChannel) {
  RegAddr prClkRam = rmt_ram_addr(eChannel, 1, 1);
  for (int i = 0; i < 9; ++i) {
    prClkRam[i] = RMT_ENTRYPAIR(0, CLK_HALFPERIOD_TICKS, 1, CLK_HALFPERIOD_TICKS);
  }
  prClkRam[9] = 0;
}

/**
 * Modifies the first 2 entry pairs of the CLK TX sequence,
 * depending on STOP / START slices to be added.
 * @param eChannel CLK RMT channel.
 * @param bStop STOP CLK slice has to be added.
 * @param bStart START CLK slice has to be added.
 */
static inline void _update_clkseq(ERmtChannel eChannel, bool bStop, bool bStart) {
  RegAddr prClkRam = rmt_ram_addr(eChannel, 1, 0);
  prClkRam[1] = RMT_ENTRYPAIR(0, CLK_HALFPERIOD_TICKS, 1, CLK_HALFPERIOD_TICKS);
  if (bStart && !bStop) {
    prClkRam[0] = RMT_ENTRYPAIR(1, 1, 1, CLK_HALFPERIOD_TICKS - 1);                     // 0.5 periods
  } else if (bStart && bStop) {
    prClkRam[0] = RMT_ENTRYPAIR(0, CLK_HALFPERIOD_TICKS, 1, 3 * CLK_HALFPERIOD_TICKS);  // 2 periods
  } else if (!bStart && bStop) {
    prClkRam[0] = RMT_ENTRYPAIR(0, CLK_HALFPERIOD_TICKS, 1, 2 * CLK_HALFPERIOD_TICKS);  // 1.5 periods
    prClkRam[1] = 0;
  } else {
    prClkRam[0] = RMT_ENTRYPAIR(0, 1, 0, 1);
    prClkRam[1] -= 2;
  }
}

/**
 * Appends DIO TX sequence of a byte (sequence of bits) to DIO RMT RAM.
 * @param prDioRam Append here.
 * @param u8Dat Byte (sequence of bits) to represent on DIO.
 */
static inline void _dat_dioseq(RegAddr prDioRam, uint8_t u8Dat) {
  static uint32_t au32EntryPairs[] = {
    RMT_ENTRYPAIR(0, CLK_PERIOD_TICKS, 0, CLK_PERIOD_TICKS),
    RMT_ENTRYPAIR(1, CLK_PERIOD_TICKS, 0, CLK_PERIOD_TICKS),
    RMT_ENTRYPAIR(0, CLK_PERIOD_TICKS, 1, CLK_PERIOD_TICKS),
    RMT_ENTRYPAIR(1, CLK_PERIOD_TICKS, 1, CLK_PERIOD_TICKS)
  };
  for (int i = 0; i < 4; ++i) {
    prDioRam[i] = au32EntryPairs[u8Dat & 0x03];
    u8Dat >>= 2;
  }
  prDioRam[4] = 0;
}

/**
 * Not a small update, but a complete rewrite of the DIO TX RMT sequence.
 * @param eChannel DIO RMT channel.
 * @param bStop STOP slice to be added.
 * @param bStart START slice to be added.
 * @param u8Dat Bit sequence to represent on DIO (except for the case when STOP is not followed by START).
 */
static inline void _update_dioseq(ERmtChannel eChannel, bool bStop, bool bStart, uint8_t u8Dat) {
  static uint32_t u32EntryPairStart = RMT_ENTRYPAIR(0, 1, 0, CLK_HALFPERIOD_TICKS - 1);                   // 0.5 periods
  static uint32_t u32EntryPairStop = RMT_ENTRYPAIR(0, 2 * CLK_HALFPERIOD_TICKS, 1, CLK_HALFPERIOD_TICKS); // 1.5 periods
  RegAddr prDioRam = rmt_ram_addr(eChannel, 1, 0);
  if (bStart && !bStop) {
    prDioRam[0] = u32EntryPairStart;
    _dat_dioseq(prDioRam + 1, u8Dat);
  } else if (bStart && bStop) {
    prDioRam[0] = u32EntryPairStop;
    prDioRam[1] = u32EntryPairStart;
    _dat_dioseq(prDioRam + 2, u8Dat);
  } else if (!bStart && bStop) {
    prDioRam[0] = u32EntryPairStop;
    prDioRam[1] = 0;
  } else {
    _dat_dioseq(prDioRam, u8Dat);
    prDioRam[0] + DIO_DELAY_TICKS;
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

  ++psParam->sByteI.u8Cur;
  rmt_start_tx(psParam->sIface.eClkCh, true);
  rmt_start_tx(psParam->sIface.eDioCh, true);

}

/**
 * Resets Byte and CmdIdx cursors, and sets their bounds.
 * @param psState
 * @param sX
 */
static void _reset_state(STm1637State *psState, SInternals sX) {
  psState->sByteI = (SRange8Idx){0, sX.u4DatE, 0, 0};
  psState->sCmdIdxI = (SRange8Idx){0, sX.u4CmdE, 0, 0};
  psState->abNak = 0;
}

/**
 * Common part of any tm1637_flush_~() functions.
 * @param psState TM1637 state.
 * @param sX Communication process length and internal boundaries.
 */
static void _start_tx_process(STm1637State *psState, SInternals sX) {
  _reset_state(psState, sX);
  _diotxend_isr(psState);
  _next_byte(psState);
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
/**
 * Initializes a state object.
 * @param psIface Defines TM1637 communication interface (GPIO pins, RMT channels).
 * @param pu8Data Source of 7 segment characters to display.
 * @return Initialized TM1637 state descriptor.
 */
STm1637State tm1637_config(const STm1637Iface *psIface, uint8_t *pu8Data) {
  return (STm1637State){
    .sIface = *psIface,
    .pu8Data = pu8Data,
    .fReadyCb = NULL,
    .pvReadyCbArg = NULL};
}

/**
 * Initializes TM1637 communication peripherals.
 * @param psState TM1637 state descriptor.
 * @param u32ApbClkFreq APB clock frequency (used to calculate RMT divisor).
 */
void tm1637_init(STm1637State *psState, uint32_t u32ApbClkFreq) {
  rmt_init_channel(psState->sIface.eClkCh, psState->sIface.u8ClkPin, true);
  rmt_init_channel(psState->sIface.eDioCh, psState->sIface.u8DioPin, true);
  _rmt_config_channel(&psState->sIface, u32ApbClkFreq / (1000 * RMT_FREQ_KHZ));
  rmt_isr_register(psState->sIface.eClkCh, RMT_INT_TXEND, _clktxend_isr, psState);
  rmt_isr_register(psState->sIface.eDioCh, RMT_INT_TXEND, _diotxend_isr, psState);
  _init_clkseq(psState->sIface.eClkCh);
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

/**
 * Sets TM1637 brightness value.
 * Note, this function does not trigger communication. Either tm1637_flush_full()
 * or tm1637_flush_brightness() must be invoked in order to apply changes.
 * @param psState TM1637 state descriptor.
 * @param bOn Turn display on (false: turn off).
 * @param u8Value Valid range: [0..7] 0: lowest (1/16), 7: highest (14/16) brightness.
 */
void tm1637_set_brightness(STm1637State *psState, bool bOn, uint8_t u8Value) {
  psState->u8Brightness = (bOn ? 0x08 : 0x00) | (u8Value & 0x07);
}

/**
 * Set callback function and parameter.
 * This callback function will be invoked when any communication process is over.
 * @param psState TM1637 state descriptor.
 * @param fHandler Callback function.
 * @param pvArg Parameter of the callback function.
 */
void tm1637_set_readycb(STm1637State *psState, Isr fHandler, void *pvArg) {
  psState->fReadyCb = fHandler;
  psState->pvReadyCbArg = pvArg;
}

/**
 * Full flush consists of 3 TM1637 commands:
 * 1. CMD_SETDATA: set write mode (with incremental addressing).
 * 2. CMD_SETADDRESS: set initial address (and write data to display registers).
 * 3. CMD_CTRLDISPLAY: set brightness.
 * @param psState TM1637 state descriptor.
 * @param u8Len Number of cells/characters to update in the display registers.
 */
void tm1637_flush_full(STm1637State *psState, uint8_t u8Len) {
  // reset address mode and address
  psState->au8Bytes[0] = CMD_SETDATA;
  psState->au8Bytes[1] = CMD_SETADDRESS;
  for (uint8_t i = 0; i < u8Len; ++i) {
    psState->au8Bytes[2 + i] = psState->pu8Data[i];
  }
  psState->au8Bytes[2 + u8Len] = CMD_CTRLDISPLAY | (psState->u8Brightness & 0x0f);
  psState->au8CmdIdx[0] = 0;
  psState->au8CmdIdx[1] = 1;
  psState->au8CmdIdx[2] = 2 + u8Len;
  _start_tx_process(psState, (SInternals){3 + u8Len, 3});
}

/**
 * Limited display register update.
 * A single command is sent (CMD_SETADDRESS) followed by data bytes.
 * @param psState TM1637 state descriptor.
 * @param u8Pos Start cell/character update at this position.
 * @param u8Len Number of data bytes to send (number of cells/characters to update).
 */
void tm1637_flush_range(STm1637State *psState, uint8_t u8Pos, uint8_t u8Len) {
  psState->au8Bytes[0] = CMD_SETADDRESS | (u8Pos & 0x07);
  for (uint8_t i = 0; i < u8Len; ++i) {
    psState->au8Bytes[1 + i] = psState->pu8Data[u8Pos + i];
  }
  psState->au8CmdIdx[0] = 0;
  _start_tx_process(psState, (SInternals){1 + u8Len, 1});
}

/**
 * Sets brightness on the TM1637 device.
 * A single command is sent (CMD_CTRLDISPLAY) without any data bytes.
 * tm1637_set_brightness() must be called before this function.
 * @param psState TM1637 state descriptor.
 */
void tm1637_flush_brightness(STm1637State *psState) {
  psState->au8Bytes[0] = CMD_CTRLDISPLAY | (psState->u8Brightness & 0x0f);
  _start_tx_process(psState, (SInternals){1, 1});
}
