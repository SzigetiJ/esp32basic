/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdint.h>
#include "bitgen.h"

// ============== Defines ==============
//============== Local types ==============

// ============== Local Data ==============
// ============== Local functions ===================
// -------------- declaration --------------------
// -------------- definition --------------------
// ============== Interface functions ==============

bool bitgen_is_low(EBitPhase ePhase) {
  return (ePhase&1) == 1;
}

SBitGenState bitgen_init(const uint8_t *pu8Begin, uint32_t u32Size) {
  SBitGenState sRet = {.pu8Begin = pu8Begin, .pu8End = pu8Begin + u32Size, .pu8Cur = pu8Begin, .u8BitIdx = -1, .eLastPhase = BIT_L0};
  return sRet;
}

EBitPhase bitgen_next(SBitGenState *psState) {
  if (bitgen_is_low(psState->eLastPhase)) {
    ++psState->u8BitIdx;
    if (psState->u8BitIdx == 8) {
      ++psState->pu8Cur;
      psState->u8BitIdx = 0;
    }
  }
  if (psState->pu8Cur == psState->pu8End) {
    psState->eLastPhase = BIT_END;
  } else {
    bool bCurBit = 0 < ((*psState->pu8Cur)&(1<<(7-psState->u8BitIdx)));
    psState->eLastPhase = bitgen_is_low(psState->eLastPhase)?(bCurBit?BIT_H1:BIT_H0):(bCurBit?BIT_L1:BIT_L0);
  }
  return psState->eLastPhase;
}

void bitgen_reset(SBitGenState *psState) {
  psState->pu8Cur = psState->pu8Begin;
  psState->u8BitIdx = -1;
  psState->eLastPhase = BIT_L0;
}

bool bitgen_end(const SBitGenState *psState) {
  return psState->pu8Cur == (psState->pu8End - 1) &&
          bitgen_is_low(psState->eLastPhase) &&
          (psState->u8BitIdx == 7);
}
