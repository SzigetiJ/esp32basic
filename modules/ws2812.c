/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdlib.h>
#include <stdbool.h>
#include "esp_attr.h"
#include "esp32types.h"
#include "rmt.h"
#include "ws2812.h"

// Timings
#define WS2812_0H_NS       400U
#define WS2812_0L_NS       850U
#define WS2812_1H_NS       800U
#define WS2812_1L_NS       450U
#define WS2812_RES_US       50U   // Reset length given in µs
#define RMT_FREQ_KHZ     20000U   // 20MHz -- clk: 50ns
#define RMT_CLK_NS            (1000000 / RMT_FREQ_KHZ)

// ================ Local function declarations =================
static void _rmt_config_channel(const SWs2812Iface *psIface, uint8_t u8Divisor);
void _byte_to_rmtram(RegAddr prDest, uint8_t u8Value);
static bool _put_next_byte(SWs2812State *psState);
static void _feeder(void *pvParam);

// =================== Global constants ================

/// This array merely stores phase length measured in RMT channel ticks.
const uint16_t gau16tckPhaseLen[] = {
  WS2812_0H_NS / RMT_CLK_NS,
  WS2812_0L_NS / RMT_CLK_NS,
  WS2812_1H_NS / RMT_CLK_NS,
  WS2812_1L_NS / RMT_CLK_NS,
  (1000 * WS2812_RES_US) / RMT_CLK_NS
};
/// In case of WS2812 the RMT phases always come in (HI,LO) pairs.
/// gau16Entries and pu32EntryPair point to the same data:
/// We use gau16Entries for data definition,
/// however, for data readout we always use pu32EntryPair.
const uint16_t gau16Entries[] = {
  // 0: 0_HI, 0_LO
  RMT_SIGNAL1 | gau16tckPhaseLen[0], RMT_SIGNAL0 | gau16tckPhaseLen[1],
  // 1: 1_HI, 1_LO
  RMT_SIGNAL1 | gau16tckPhaseLen[2], RMT_SIGNAL0 | gau16tckPhaseLen[3]
};
/// pu32EntryPair points to an array of two items:
/// pu32EntryPair[0] stores the RMT entrypair associated with bit0, whereas
/// pu32EntryPair[1] stores the RMT entrypair associated with bit1
const uint32_t *pu32EntryPair = (const uint32_t*)gau16Entries;

// ==================== Local Data ================

// ==================== Implementation ================

static void _rmt_config_channel(const SWs2812Iface *psIface, uint8_t u8Divisor) {
  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = u8Divisor, .u4MemSize = psIface->u8Blocks, .bCarrierEn = false, .bCarrierOutLvl = 1},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1, .bIdleOutLvl = 0, .bIdleOutEn = 0}
  };
  gpsRMT->asChConf[psIface->eChannel] = rChConf;

  // set memory ownership of RMT RAM blocks
  SRmtChConf1Reg sRdMemCfg = {.raw = -1};
  sRdMemCfg.bMemOwner = 0;
  for (int i = 0; i < psIface->u8Blocks; ++i) {
    gpsRMT->asChConf[(psIface->eChannel + i) % RMT_CHANNEL_NUM].r1.raw &= sRdMemCfg.raw;
  }

  gpsRMT->arTxLim[psIface->eChannel].u9Val = (psIface->u8Blocks * RMT_RAM_BLOCK_SIZE) / 2;
}

/**
 * Generates RMT entry pairs for the 8 bits of a byte and puts them into the RMT RAM.
 * @param prDest Put the entry pair to the registers starting at this address.
 * @param u8Value The byte to process.
 */
IRAM_ATTR void _byte_to_rmtram(RegAddr prDest, uint8_t u8Value) {
  for (int i = 0; i < 8; ++i) {
    uint8_t u8EntryPatternIdx = (u8Value >> (8 - 1 - i)) & 1;
    prDest[i] = pu32EntryPair[u8EntryPatternIdx];
  }
}

/**
 * Wrapper function to _byte_to_rmtram().
 * If all the bytes of the input data are sent, the terminating entry pair
 * is put into the current RMT RAM register.
 * @param psState State descriptor.
 * @return End of the input data is still not reached.
 */
IRAM_ATTR static bool _put_next_byte(SWs2812State *psState) {
  uint32_t u32Offset = 8 * psState->szPos;
  RegAddr prDest = rmt_ram_addr(psState->sIface.eChannel, psState->sIface.u8Blocks, u32Offset);
  if (psState->szPos < psState->szLen) {
    _byte_to_rmtram(prDest, psState->pu8Data[psState->szPos++]);
  } else {
    prDest[0] = 0;
    return false;
  }
  return true;
}

/**
 * The feeder procedure fills half of the RMT RAM range assigned to the channel.
 * This function is called twice before starting the RMT TX procedure (thus fills the whole RAM range),
 * and it is also triggered by the TXTHRES RMT interrupt.
 * @param pvParam SWs2812State pointer.
 */
static void _feeder(void *pvParam) {
  SWs2812State *psParam = (SWs2812State*)pvParam;

  for (int i = 0; i < (psParam->sIface.u8Blocks * RMT_RAM_BLOCK_SIZE) / 16; ++i) {
    if (!_put_next_byte(psParam)) break;
  }
}

// ============== Interface functions ==============

void ws2812_init(uint8_t u8Pin, uint32_t u32ApbClkFreq, SWs2812State *psFeederState, Isr fTxEndCb, void *pvTxEndCbParam) {
  rmt_init_channel(psFeederState->sIface.eChannel, u8Pin, false);
  _rmt_config_channel(&psFeederState->sIface, u32ApbClkFreq / (1000 * RMT_FREQ_KHZ));
  rmt_isr_register(psFeederState->sIface.eChannel, RMT_INT_TXTHRES, _feeder, psFeederState);
  rmt_isr_register(psFeederState->sIface.eChannel, RMT_INT_TXEND, fTxEndCb, pvTxEndCbParam);
}

void ws2812_start(SWs2812State *psFeederState) {
  psFeederState->szPos = 0;
  _feeder(psFeederState);
  _feeder(psFeederState);
  rmt_start_tx(psFeederState->sIface.eChannel, true);
}
