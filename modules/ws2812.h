/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef WS2812_H
#define WS2812_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rmt.h"

  // ============= Types ===============

/**
 * Whenever we send data to WS2812, first we have to know which RMT channel we use.
 * Also, we have to know the register range where to put the data.
 */
  typedef struct {
    ERmtChannel eChannel; ///< Identifies the RMT channel
    uint8_t u8Blocks;     ///< Number of RMT RAM blocks the channel owns
  } SWs2812Iface;

/**
 * State descriptor of the RMT data transmission for WS2812.
 */
  typedef struct {
    uint8_t *pu8Data;     ///< Points to data (GRB sequence) (Set by user)
    size_t szLen;         ///< Length of the data (number of bytes)
    size_t szPos;         ///< Current data cursor (Internal attribute)
    SWs2812Iface sIface;  ///< Interface description
  } SWs2812State;

  // ============= Inline functions ===============
  /**
   * Initializes a state descriptor with the given attributes.
   * @param pu8Data Source data.
   * @param szLen Source data length.
   * @param eChannel RMT channel.
   * @param u8Blocks Number of RMT RAM blocks the channel owns.
   * @return Initialized state descriptor.
   */
  static inline SWs2812State ws2812_init_feederstate(uint8_t *pu8Data, size_t szLen, ERmtChannel eChannel, uint8_t u8Blocks) {
    SWs2812Iface sIface = {
      .eChannel = eChannel,
      .u8Blocks = u8Blocks
    };
    return (SWs2812State){
      .pu8Data = pu8Data,
      .szLen = szLen,
      .szPos = 0,
      .sIface = sIface};
  }
  // ============= Interface function declaration ===============

  void ws2812_init(uint8_t u8Pin, uint32_t u32ApbClkFreq, SWs2812State *psFeederState, Isr fTxEndCb, void *pvTxEndCbParam);
  void ws2812_start(SWs2812State *psFeederState);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_H */

