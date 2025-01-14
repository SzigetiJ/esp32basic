/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <string.h>

#include "rmtutils.h"

// ============== Internal function declarations ==============
static uint32_t _pairgen_next(U16Generator pfGen, UniRel pfEnd, void *pvParam);
static uint16_t _stretchgen_next(void *pvState);
static bool _stretchgen_end(const void *pvState);


// ============== Internal functions ==============

/**
 * Makes a uint32_t value from two uint16_t values taken from pfGen.
 * @param pfGen Underlying source: the function generating the uint16_t values.
 * @param pfEnd End of sequence indicator function of the uint16_t source.
 * @param pvParam Parameter to pass to pfGen and pfEnd functions.
 * @return Bits 0..15 contain the first value received from the generator,
 * bits 16.,31 depend on pfEnd: if end of sequence is true after the first value is read,
 * 0; otherwise these bits contain the second value received from the underlying generator.
 */
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

static bool _stretchgen_end(const void *pvParam) {
  const SStretchGenState *psParam = (const SStretchGenState*) pvParam;
  return psParam->fGenEnd(psParam->pvGenParam) && (psParam->u32OutQueue == 0);
}

// ============== Interface functions ==============

uint32_t rmtutils_copytoram(ERmtChannel eChannel, uint8_t u8Blocks, uint32_t u32Offset, uint32_t *pu32Src, uint32_t u32Len) {
  for (uint32_t i = 0; i < u32Len; ++i) {
    *rmt_ram_addr(eChannel, u8Blocks, u32Offset + i) = pu32Src[i];
  }

  return u32Offset + u32Len;
}

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
  uint8_t u8Blocks = gpsRMT->asChConf[eChannel].r0.u4MemSize;
  uint16_t u16Written = 0; // counter for written registers
  bool bRet = false;

  for (; u16Written < u16Len && !bRet; ++u16Written) {
    uint32_t u32RegValue = pfEnd(pvGen) ? 0 : _pairgen_next(pfGen, pfEnd, pvGen); // entry-pair to write to the RMT RAM register

    *rmt_ram_addr(eChannel, u8Blocks, *pu16MemPos + u16Written) = u32RegValue;

    // check if tx termination value was written
    if ((u32RegValue & RMT_ENTRYMAX) == 0 || (u32RegValue & (RMT_ENTRYMAX << 16)) == 0) {
      bRet = true;
    }
  }
  *pu16MemPos += u16Written;
  *pu16MemPos %= (u8Blocks * RMT_RAM_BLOCK_SIZE);
  return bRet;
}
