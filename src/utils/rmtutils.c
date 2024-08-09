/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include "rmtutils.h"

// ============== Internal function declarations ==============
static uint32_t _pairgen_next(U16Generator pfGen, UniRel pfEnd, void *pvParam);
static uint16_t _stretchgen_next(void *pvState);
static bool _stretchgen_end(void *pvState);


// ============== Internal functions ==============

static uint32_t _pairgen_next(U16Generator pfGen, UniRel pfEnd, void *pvParam) {
  uint16_t au16Val[] = {0, 0};
  uint32_t *pu32Val = (uint32_t*) au16Val;

  au16Val[0] = pfGen(pvParam);
  if (!pfEnd(pvParam)) {
    au16Val[1] = pfGen(pvParam);
  }
  return *pu32Val;
}

static uint16_t _stretchgen_next(void *pvParam) {
  SStretchGenState *psParam = (SStretchGenState*) pvParam;
  uint16_t u16Ret;
  if (0 == psParam->u32OutQueue) {
    uint16_t u16Val = psParam->fGen(psParam->pvGenParam);
    psParam->bLevel = 0 < (u16Val & RMT_SIGNAL1);
    psParam->u32OutQueue = (u16Val & RMT_ENTRYMAX) * psParam->u32Multiplier;
    psParam->u32OutQueue /= psParam->u32Divisor;
  }
  u16Ret = (RMT_ENTRYMAX < psParam->u32OutQueue) ? RMT_ENTRYMAX : psParam->u32OutQueue;
  psParam->u32OutQueue -= u16Ret;
  u16Ret |= (psParam->bLevel ? RMT_SIGNAL1 : RMT_SIGNAL0);
  return u16Ret;
}

static bool _stretchgen_end(void *pvParam) {
  SStretchGenState *psParam = (SStretchGenState*) pvParam;
  return psParam->fGenEnd(psParam->pvGenParam) && (psParam->u32OutQueue == 0);
}

// ============== Interface functions ==============

SStretchGenState rmtutils_init_stretchgenstate(uint32_t u32Multiplier, uint32_t u32Divisor, U16Generator fGen,
        UniRel fGenEnd, void *pvGenParam) {
  SStretchGenState sRet = {
    .fGen = fGen,
    .fGenEnd = fGenEnd,
    .pvGenParam = pvGenParam,
    .u32Multiplier = u32Multiplier,
    .u32Divisor = u32Divisor,
    .u32OutQueue = 0,
    .bLevel = false
  };
  return sRet;
}

bool rmtutils_feed_tx_stretched(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, SStretchGenState *psSGenState) {
  return rmtutils_feed_tx(eChannel, pu16MemPos, u16Len, _stretchgen_next, _stretchgen_end, psSGenState);
}

bool rmtutils_feed_tx(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, U16Generator pfGen, UniRel pfEnd, void *pvGen) {
  RegAddr prMemBase0 = rmt_ram_block(0);
  uint32_t u32MemChOffset = eChannel * RMT_RAM_BLOCK_SIZE;
  uint8_t u8Blocks = gpsRMT->asChConf[eChannel].r0.u4MemSize;
  uint16_t u16Offset = 0;
  bool bRet = false;
  for (; u16Offset < u16Len && !bRet; ++u16Offset) {
    uint32_t u32Tmp = pfEnd(pvGen) ? 0 : _pairgen_next(pfGen, pfEnd, pvGen);
    if (u32Tmp == 0) {
      bRet = true;
    }
    prMemBase0[(((*pu16MemPos + u16Offset) % (u8Blocks * RMT_RAM_BLOCK_SIZE)) + u32MemChOffset) % (8 * RMT_RAM_BLOCK_SIZE)] = u32Tmp;
  }
  *pu16MemPos += u16Offset;
  *pu16MemPos %= (u8Blocks * RMT_RAM_BLOCK_SIZE);
  return bRet;
}
