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

  typedef struct {
    ERmtChannel eChannel;
    uint8_t u8Blocks;
  } SWs2812Iface;

  typedef struct {
    uint8_t *pu8Data;
    size_t szLen;
    size_t szPos;
    bool bBusy;
    SWs2812Iface sIface;
  } SWs2812FeederState;

  // ============= Interface function declaration ===============

  void ws2812_init(uint8_t u8Pin, uint32_t u32ApbClkFreq, Isr fTxEndCb, SWs2812FeederState *psFeederState);
  void ws2812_start(SWs2812FeederState *psFeederState);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_H */

