/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include "generators.h"

#define BIT_END 8
#define PWMPHASE_END 2

// ============= Global constants ===============
const SToByteFunctions gsByteGenFunc = {
  .fNext = (FToByteNext) bytegen_next,
  .fEnd = (FToXEnd) bytegen_end,
  .fReset = (FToXReset) bytegen_reset
};

const SToByteFunctions gsBitGenFunc = {
  .fNext = (FToByteNext) bitgen_next,
  .fEnd = (FToXEnd) bitgen_end,
  .fResetV = (FByteToXResetV) bitgen_resetv
};

const SToByteFunctions gsBitSeqGenFunc = {
  .fNext = (FToByteNext) trbytegen_next,
  .fEnd = (FToXEnd) trbytegen_end,
  .fReset = (FToXReset) trbytegen_reset
};

const SToWordFunctions gsPwmGenFunc = {
  .fNext = (FToWordNext) pwmgen_next,
  .fEnd = (FToXEnd) pwmgen_end,
  .fReset = (FToXReset) pwmgen_reset
};

// ============= Implementation ===============
// ------------- Interface functions ---------------

// Byte Generator section

/**
 * Initializes a Byte Generator state descriptor.
 * @param pu8Seq Underlying data array to iterate on.
 * @param szSeqLen Size of the data array.
 * @return Initialized state descriptor.
 */
SByteGenState bytegen_init(const uint8_t *pu8Seq, size_t szSeqLen) {
  return (SByteGenState){
    .pu8Begin = pu8Seq,
    .pu8Cur = pu8Seq,
    .pu8End = pu8Seq + szSeqLen};
}

uint8_t bytegen_next(SByteGenState *psState) {
  return *(psState->pu8Cur++);
}

void bytegen_reset(SByteGenState *psState) {
  psState->pu8Cur = psState->pu8Begin;
}

bool bytegen_end(const SByteGenState *psState) {
  return psState->pu8Cur == psState->pu8End;
}


// Bit Generator section

SBitGenState bitgen_init(uint8_t u8Value, bool bUp, uint8_t u8OutHi, uint8_t u8OutLo) {
  return (SBitGenState){
    .u8Value = u8Value,
    .u8BitIdx = 0,
    .bUp = bUp,
    .u8OutHi = u8OutHi,
    .u8OutLo = u8OutLo};
}

uint8_t bitgen_next(SBitGenState *psState) {
  uint8_t u8Mask = 1 << (psState->bUp ? psState->u8BitIdx : (BIT_END - 1 - psState->u8BitIdx));
  ++psState->u8BitIdx;
  return (psState->u8Value & u8Mask) ? psState->u8OutHi : psState->u8OutLo;
}

bool bitgen_end(const SBitGenState *psState) {
  return psState->u8BitIdx == BIT_END;
}

void bitgen_resetv(SBitGenState *psState, uint8_t u8Value) {
  psState->u8Value = u8Value;
  psState->u8BitIdx = 0;
}

// Transformed Byte Generator section

uint8_t trbytegen_next(STrByteGenState *psState) {
  bool bNextA = psState->psFuncB->fEnd(psState->pvBState);
  if (bNextA) {
    psState->psFuncB->fResetV(psState->pvBState, psState->psFuncA->fNext(psState->pvAState));
  }
  return psState->psFuncB->fNext(psState->pvBState);
}

bool trbytegen_end(const STrByteGenState *psState) {
  return psState->psFuncB->fEnd(psState->pvBState) && psState->psFuncA->fEnd(psState->pvAState);
}

void trbytegen_reset(STrByteGenState *psState) {
  psState->psFuncA->fReset(psState->pvAState);
  while (!psState->psFuncB->fEnd(psState->pvBState)) psState->psFuncB->fNext(psState->pvBState);
}

// Bit Sequence Generator section

STrByteGenState bitseqgen_init(SByteGenState *psAState, SBitGenState *psBState) {
  STrByteGenState sRet = {
    .pvAState = psAState,
    .pvBState = psBState,
    .psFuncA = &gsByteGenFunc,
    .psFuncB = &gsBitGenFunc
  };
  return sRet;
}

// PWM Generator section

SPwmGenState pwmgen_init(void *pvState, const SToByteFunctions *psFunc, uint8_t u8PeriodLen, uint8_t u8HiUpper, uint8_t u8LoUpper) {
  SPwmGenState sRet = {
    .pvAState = pvState,
    .psFuncA = psFunc,
    .u8CurValue = 0,
    .u8PeriodLen = u8PeriodLen,
    .u8HiUpper = u8HiUpper,
    .u8LoUpper = u8LoUpper,
    .u8PhaseIdx = PWMPHASE_END
  };
  return sRet;
}

uint16_t pwmgen_next(SPwmGenState *psState) {
  if (psState->u8PhaseIdx == PWMPHASE_END) {
    psState->u8PhaseIdx = 0;
    psState->u8CurValue = psState->psFuncA->fNext(psState->pvAState);
  }
  uint8_t u8RetLower = (psState->u8PhaseIdx == 0 ? psState->u8CurValue : (psState->u8PeriodLen - psState->u8CurValue));
  uint8_t u8RetUpper = psState->u8PhaseIdx == 0 ? psState->u8HiUpper : psState->u8LoUpper;

  ++psState->u8PhaseIdx;
  return (u8RetUpper << BIT_END) | u8RetLower;
}

bool pwmgen_end(const SPwmGenState *psState) {
  return (psState->u8PhaseIdx == PWMPHASE_END) && psState->psFuncA->fEnd(psState->pvAState);
}

void pwmgen_reset(SPwmGenState *psState) {
  psState->psFuncA->fReset(psState->pvAState);
  psState->u8PhaseIdx = PWMPHASE_END;
}

// (Faster) PWM Generator section

uint16_t bitpwmgen_next(SBitPwmGenState *psState) {
  SPwmXGenState *px = &psState->sPwmXGenState; // alias, for writing shorter lines
  if (px->u8PhaseIdx == PWMPHASE_END) {
    if (bitgen_end(&psState->sBitGenState)) {
      bitgen_resetv(&psState->sBitGenState, bytegen_next(&psState->sByteGenState));
    }
    px->u8CurValue = bitgen_next(&psState->sBitGenState);
    px->u8PhaseIdx = 0;
  }
  uint8_t u8RetUpper = (px->u8PhaseIdx == 0) ? px->u8HiUpper : px->u8LoUpper;
  uint8_t u8RetLower = (px->u8PhaseIdx == 0) ? px->u8CurValue : px->u8PeriodLen - px->u8CurValue;
  ++px->u8PhaseIdx;
  return u8RetUpper << BIT_END | u8RetLower;
}

bool bitpwmgen_end(const SBitPwmGenState *psState) {
  return bytegen_end(&psState->sByteGenState) && bitgen_end(&psState->sBitGenState) && psState->sPwmXGenState.u8PhaseIdx == PWMPHASE_END;
}

void bitpwmgen_reset(SBitPwmGenState *psState) {
  bytegen_reset(&psState->sByteGenState);
  bitgen_resetv(&psState->sBitGenState, 0);
  psState->sBitGenState.u8BitIdx = BIT_END;
  psState->sPwmXGenState.u8PhaseIdx = PWMPHASE_END;
}
