/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stdbool.h>
#include <string.h>

#include "dht22.h"
#include "rmt.h"

#define DHT_HOSTPULLDOWN_IVAL_US 1100U  ///< The communication begins with host sending out signal 0 for 1.1 ms.

// measured: [70..73] and [23..27]
#define DHT_BIT1_IVAL_LO_US 68
#define DHT_BIT1_IVAL_HI_US 75
#define DHT_BIT0_IVAL_LO_US 22
#define DHT_BIT0_IVAL_HI_US 29

#define DHT22_IDLE_US 90            ///< If the input signal is constant 0 or 1 for more than 90 µs, the RX process is done.

#define RMT_FREQ_KHZ     1000U      ///< 1MHz -- clk: 1µs
#define RMT_CLK_NS            (1000000 / RMT_FREQ_KHZ)
#if (1000 == RMT_FREQ_KHZ)
#define US_TO_RMTCLK(X) (X)
#define RMTCLK_TO_US(X) (X)
#else
#define US_TO_RMTCLK(X) (((X) * RMT_FREQ_KHZ) / 1000)
#define RMTCLK_TO_US(X) (((X) * 1000) / RMT_FREQ_KHZ)
#endif
#define RMTDHT_FILTER_THRES 50U     ///< RMT filters out spikes shorter than that many APB clocks.

// ================ Local function declarations =================
static inline bool _u16ltz(uint16_t u16Value);
static inline int16_t _u16abs(uint16_t u16Value);
static inline int16_t _u16toi16(uint16_t u16Value);
static void _rmt_config_channel(const ERmtChannel eChannel, uint8_t u8Divisor);
static void _rxstart(void *pvParam);
static void _rxready(void *pvParam);
// =================== Global constants ================

// ==================== Local Data ================

// ==================== Implementation ================
static inline bool _u16ltz(uint16_t u16Value) {
  return (u16Value & (1 << 15));
}

static inline int16_t _u16abs(uint16_t u16Value) {
  return (u16Value & 0x7fff);
}

static inline int16_t _u16toi16(uint16_t u16Value) {
  return _u16ltz(u16Value) ?
          -_u16abs(u16Value) :
          u16Value;
}

static void _rmt_config_channel(ERmtChannel eChannel, uint8_t u8Divisor) {
  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = u8Divisor, .u4MemSize = 1,
      .u16IdleThres = US_TO_RMTCLK(DHT22_IDLE_US)},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1,
      .bIdleOutLvl = 1, .bIdleOutEn = 1,
      .bRxFilterEn = 1, .u8RxFilterThres = RMTDHT_FILTER_THRES}
  };
  gpsRMT->asChConf[eChannel] = rChConf;

  gpsRMT->arTxLim[eChannel].u9Val = 256; // currently unused
}

static void _rxstart(void *pvParam) {
  SDht22Descriptor *psParam = (SDht22Descriptor*)pvParam;
  rmt_start_rx(psParam->eChannel, 1);

}

static void _rxready(void *pvParam) {
  SDht22Descriptor *psParam = (SDht22Descriptor*)pvParam;
  // Note, here we use _absolute_ RMT RAM addresses.
  // This works fine since the received data fits into a single RMT RAM block.
  RegAddr prData = rmt_ram_block(RMT_CH0);
  uint32_t u32RecvEnd = gpsRMT->asStatus[psParam->eChannel].u9RxIdx;
  uint32_t u32DataOfs = u32RecvEnd - (DHT22_DATA_LEN * 8) - 1;
  bool bLowEnd = (prData[u32RecvEnd - 1] & RMT_ENTRYMAX) == 0;
  uint8_t u8Shr = bLowEnd ? 0 : 16;

  memset(&psParam->sData, 0, sizeof (psParam->sData));

  for (int i = 0; i < (DHT22_DATA_LEN * 8); ++i) {
    int iByte = i / 8;
    int iBit = 7 - (i % 8);
    uint32_t u32Dat = (prData[u32DataOfs + i]) >> u8Shr;
    bool bLevel0 = (0 != (u32Dat & RMT_SIGNAL1));
    uint16_t u16Duration0 = RMTCLK_TO_US(u32Dat & RMT_ENTRYMAX);
    bool bValid = bLevel0;
    bool bValue = false;
    if (DHT_BIT0_IVAL_LO_US <= u16Duration0 && u16Duration0 <= DHT_BIT0_IVAL_HI_US) {
      bValue = false;
    } else if (DHT_BIT1_IVAL_LO_US <= u16Duration0 && u16Duration0 <= DHT_BIT1_IVAL_HI_US) {
      bValue = true;
    } else {
      bValid = false;
    }
    if (bValue)
      psParam->sData.au8Data[iByte] |= (1 << iBit);
    if (!bValid)
      psParam->sData.au8Invalid[iByte] |= (1 << iBit);
  }

  psParam->fReadyCb(psParam->pvReadyCbParam, &psParam->sData);
}

// ============== Interface functions ==============

/**
 * Initializes DHT22 communication environment.
 * @param u8Pin GPIO pin.
 * @param u32ApbClkFreq APB clock frequency.
 * @param psDht22Desc DHT22 communication descriptor.
 */
void dht22_init(uint8_t u8Pin, uint32_t u32ApbClkFreq, SDht22Descriptor *psDht22Desc) {
  rmt_init_channel(psDht22Desc->eChannel, u8Pin, false);
  _rmt_config_channel(psDht22Desc->eChannel, u32ApbClkFreq / (1000 * RMT_FREQ_KHZ));
  rmt_isr_register(psDht22Desc->eChannel, RMT_INT_TXEND, _rxstart, psDht22Desc);
  rmt_isr_register(psDht22Desc->eChannel, RMT_INT_RXEND, _rxready, psDht22Desc);
}

/**
 * Starts a DHT22 communication process.
 * @param psDht22Desc DHT22 communication descriptor.
 */
void dht22_run(SDht22Descriptor *psDht22Desc) {
  static const uint32_t u32Tx0 = (RMT_SIGNAL0 | US_TO_RMTCLK(DHT_HOSTPULLDOWN_IVAL_US)) | (RMT_SIGNAL1 << 16);

  SRmtChConf1Reg sConf1 = {.raw = 0};
  sConf1.bMemOwner = 1;
  gsRMT.asChConf[psDht22Desc->eChannel].r1.raw |= sConf1.raw;

  rmt_ram_addr(psDht22Desc->eChannel, 1, 0)[0] = u32Tx0;

  rmt_start_tx(psDht22Desc->eChannel, 1);
}

/**
 * Gets humidity data from received and bit-decoded data.
 * @param psData Received bit-decoded data.
 * @return Humidity data with 1-digit precision.
 */
uint16_t dht22_get_rhum(const SDht22Data *psData) {
  return (psData->au8Data[0] << 8) | psData->au8Data[1];
}

/**
 * Gets temperature data from received and bit-decoded data.
 * @param psData Received bit-decoded data.
 * @return Temperature data with 1-digit precision.
 */
int16_t dht22_get_temp(const SDht22Data *psData) {
  uint16_t u16T0 = (psData->au8Data[2] << 8) | psData->au8Data[3];
  return _u16toi16(u16T0);
}

/**
 * Tells if the received data is valid.
 * @param psData Received bit-decoded data.
 * @return The data does not contains any invalid bit and the checksum matches.
 */
bool dht22_data_valid(const SDht22Data *psData) {
  bool bAllBitValid = true;
  uint8_t u8DataSum = 0;

  for (int i = 0; i < DHT22_DATA_LEN; ++i) {
    bAllBitValid &= (psData->au8Invalid[i] == 0);
  }
  for (int i = 0; i < DHT22_DATA_LEN - 1; ++i) {
    u8DataSum += psData->au8Data[i];
  }
  bool bCheckSum = (u8DataSum == psData->au8Data[DHT22_DATA_LEN - 1]);
  return bAllBitValid && bCheckSum;
}

