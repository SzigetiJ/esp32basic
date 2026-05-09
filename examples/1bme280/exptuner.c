/*
 * Copyright 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include "exptuner.h"
#include "typeaux.h"

// ============== Defines ==============

// ============= Local types ===============

// ==================== Local data ================
const uint8_t gau8B10Mul1[] = {1, 2, 5};
const uint8_t gau8B10Mul2[] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

// ============== Internal function declarations ==============
static inline void _step_level1(const SExpTuner2Desc *psTuner, SExpTuner2Value *psValue, bool bUp);
static inline void _step_level2(const SExpTuner2Desc *psTuner, SExpTuner2Value *psValue, bool bUp);

// ============== Implementation ==============
// -------------- Internal functions --------------

static inline void _step_level1(const SExpTuner2Desc *psTuner, SExpTuner2Value *psValue, bool bUp) {
  if (bUp) {
    ++psValue->u8Mul1Idx;
    if (psTuner->u8Mul1Size <= psValue->u8Mul1Idx) {
      psValue->u8Mul1Idx = 0;
      psValue->u32Base *= psTuner->u8Base;
    }
  } else {  // down
    if (0 < psValue->u8Mul2Idx) {
    } else if (0 == psValue->u8Mul1Idx) {
      psValue->u8Mul1Idx = psTuner->u8Mul1Size - 1;
      psValue->u32Base /= psTuner->u8Base;
    } else {
      --psValue->u8Mul1Idx;
    }
  }
  psValue->u8Mul2Idx = 0;
}

static inline void _step_level2(const SExpTuner2Desc *psTuner, SExpTuner2Value *psValue, bool bUp) {
  if (bUp) {
    uint32_t u32L1Next = ((psValue->u8Mul1Idx == psTuner->u8Mul1Size - 1) ? psTuner->u8Base : psTuner->puMul1[psValue->u8Mul1Idx + 1]) * psTuner->puMul2[0];
    ++psValue->u8Mul2Idx;
    if ((psTuner->u8Mul2Size <= psValue->u8Mul2Idx) || (u32L1Next <= psTuner->puMul1[psValue->u8Mul1Idx] * psTuner->puMul2[psValue->u8Mul2Idx])) {
      _step_level1(psTuner, psValue, true);
    }
  } else {  // down
    if (psValue->u8Mul2Idx == 0) {
      uint32_t u32Curr = ((psValue->u8Mul1Idx == 0) ? psTuner->u8Base : 1) * psTuner->puMul1[psValue->u8Mul1Idx] * psTuner->puMul2[0];
      _step_level1(psTuner, psValue, false);
      psValue->u8Mul2Idx = psTuner->u8Mul2Size - 1;
      while (psValue->u8Mul2Idx && (u32Curr <= psTuner->puMul1[psValue->u8Mul1Idx] * psTuner->puMul2[psValue->u8Mul2Idx])) {
        --psValue->u8Mul2Idx;
      }
    } else {
      --psValue->u8Mul2Idx;
    }
  }
}

// -------------- Interface functions --------------

/**
 * Get a fairly balanced tuner descriptor with base 10 and a tuning granurality of 4 -- 10 %.
 * @return 
 */
SExpTuner2Desc exptuner2_init_b10() {
  SExpTuner2Desc sRet = {
    .u8Base = 10,
    .u8FinalDiv = 1,
    .u8Mul1Size = ARRAY_SIZE(gau8B10Mul1),
    .u8Mul2Size = ARRAY_SIZE(gau8B10Mul2),
    .puMul1 = gau8B10Mul1,
    .puMul2 = gau8B10Mul2
  };
  return sRet;
}

/**
 * Tune up/down an SExpTuner2Value by exactly a single step.
 * @param psTuner Tuner descriptor.
 * @param psLowerBound The value must not go below this limit.
 * @param psUpperBound The value must not go above this limit.
 * @param psValue Value to tune.
 * @param bUp Tune to higher (true) or lower (false) value.
 * @param bSoft Choose between level2 (true) or level1 (false) tuning.
 * @return true: value modified, false: value not modified (lower or upper limit reached).
 */
bool exptuner2_step(const SExpTuner2Desc *psTuner, const SExpTuner2Value *psLowerBound, const SExpTuner2Value *psUpperBound, SExpTuner2Value *psValue, bool bUp, bool bSoft) {
  bool bRet = true;
  if (bSoft) {
    _step_level2(psTuner, psValue, bUp);
  } else {
    _step_level1(psTuner, psValue, bUp);
  }
  if (bUp && exptuner2_get(psTuner, psUpperBound) <= exptuner2_get(psTuner, psValue)) {
    *psValue = *psUpperBound;
    bRet = false;
  }
  if (!bUp && exptuner2_get(psTuner, psValue) <= exptuner2_get(psTuner, psLowerBound)) {
    *psValue = *psLowerBound;
    bRet = false;
  }
  return bRet;
}

/**
 * Convert SExpTuner2Value to uint32_t.
 * @param psTuner Tuner descriptor.
 * @param psValue Value to convert.
 * @return Converted value.
 */
uint32_t exptuner2_get(const SExpTuner2Desc *psTuner, const SExpTuner2Value * psValue) {
  return (psValue->u32Base * psTuner->puMul1[psValue->u8Mul1Idx] * psTuner->puMul2[psValue->u8Mul2Idx]);
}

/**
 * Get the greatest SExpTuner2Value that is not greater than u32Value.
 * @param psTuner Tuner descriptor.
 * @param u32Value Value to approximate.
 * @return SExpTuner2Value approximation of u32Value from below.
 */
SExpTuner2Value exptuner2_lower_bound(const SExpTuner2Desc *psTuner, uint32_t u32Value) {
  SExpTuner2Value sRet = {.u32Base = 1, .u8Mul1Idx = 0, .u8Mul2Idx = 0};
  // tune base
  while (true) {
    SExpTuner2Value sCopy = sRet;
    sCopy.u32Base *= psTuner->u8Base;
    if (u32Value < exptuner2_get(psTuner, &sCopy)) break;
    sRet.u32Base = sCopy.u32Base;
  }

  // tune mul1
  while (true) {
    SExpTuner2Value sCopy = sRet;
    ++sCopy.u8Mul1Idx;
    if (psTuner->u8Mul1Size <= sCopy.u8Mul1Idx || u32Value < exptuner2_get(psTuner, &sCopy)) break;
    sRet.u8Mul1Idx = sCopy.u8Mul1Idx;
  }

  // tune mul1
  while (true) {
    SExpTuner2Value sCopy = sRet;
    ++sCopy.u8Mul2Idx;
    if (psTuner->u8Mul2Size <= sCopy.u8Mul2Idx || u32Value < exptuner2_get(psTuner, &sCopy)) break;
    sRet.u8Mul2Idx = sCopy.u8Mul2Idx;
  }
  return sRet;
}


SExpTuner2Desc exptuner2_init_b10d() {
  SExpTuner2Desc sRet = exptuner2_init_b10();
  sRet.u8FinalDiv = 10;
  return sRet;
}

SExpTuner2Value exptuner2d_lower_bound(const SExpTuner2Desc *psTuner, uint32_t u32Value) {
  return exptuner2_lower_bound(psTuner, psTuner->u8FinalDiv * u32Value);
}

bool exptuner2d_step(const SExpTuner2Desc *psTuner, const SExpTuner2Value *psLowerBound, const SExpTuner2Value *psUpperBound, SExpTuner2Value *psValue, bool bUp, bool bSoft) {
  uint32_t u32Orig = exptuner2d_get(psTuner, psValue);
  bool bRet = false;
  while (exptuner2_step(psTuner, psLowerBound, psUpperBound, psValue, bUp, bSoft)) {
    if (exptuner2d_get(psTuner, psValue) != u32Orig) {
      bRet = true;
      break;
    }
  }
  return bRet;
}

uint32_t exptuner2d_get(const SExpTuner2Desc *psTuner, const SExpTuner2Value * psValue) {
  return exptuner2_get(psTuner, psValue) / psTuner->u8FinalDiv;
}
