#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "utils/generators.h"
#include "typeaux.h"
#include "gentest_common.h"

typedef struct {
  bool bLsbFirst;
  uint8_t u8High;
  uint8_t u8Low;
} SBitGenInitParam;

// bitgen test data
const uint8_t gu8Input0 = 0xA4;
const SBitGenInitParam gasParam[] = {
  {false, 1, 0},
  {false, 2, 4},
  {true, 1, 0}
};
const uint8_t gau8Exp0_0[] = {1, 0, 1, 0, 0, 1, 0, 0};
const uint8_t gau8Exp0_1[] = {2, 4, 2, 4, 4, 2, 4, 4};
const uint8_t gau8Exp0_2[] = {0, 0, 1, 0, 0, 1, 0, 1};

// bitseqgen test data
const uint8_t gau8Input1[] = {0x80, 0xff};
const SBitGenInitParam gsParam1 = {false, 1, 0};
const uint8_t gau8Expected1[] = {1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};

const uint8_t gau8Input3[] = {0x80, 0x00, 0xA5, 0xA5};
const uint8_t gau8ResetAt3[] = {6, 8, 10, 12, 14};
const uint8_t gau8GenLen3[] = {21, 21, 21, 21, 21};
const uint8_t gau8Expected3[5][21] = {
  {1, 0, 0, 0, 0, 0,  1, 0, 1, 0, 0, 1, 0, 1,  1, 0, 1, 0, 0, 1, 0},
  {1, 0, 0, 0, 0, 0, 0, 0,  1, 0, 1, 0, 0, 1, 0, 1,  1, 0, 1, 0, 0},
  {1, 0, 0, 0, 0, 0, 0, 0,  1, 0,  1, 0, 1, 0, 0, 1, 0, 1,  1, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 0,  1, 0, 0, 0,  1, 0, 1, 0, 0, 1, 0, 1,  1},
  {1, 0, 0, 0, 0, 0, 0, 0,  1, 0, 0, 0, 0, 0,  1, 0, 1, 0, 0, 1, 0}
};

typedef struct {
  const uint8_t *pu8Begin;
  const uint8_t *pu8End;
  const uint8_t *pu8Cur;
  uint8_t u8Value;
  uint8_t u8BitIdx;
  bool bUp;
  uint8_t u8OutHi;
  uint8_t u8OutLo;
} SBitSGenState;

SBitSGenState bitsgen_init(const uint8_t *pu8Array, size_t szArrayLen, bool bUp, uint8_t u8OutHi, uint8_t u8OutLo) {
  SBitSGenState sRet = {
    .pu8Begin = pu8Array,
    .pu8End = pu8Array + szArrayLen,
    .pu8Cur = pu8Array,
    .u8Value = 0,
    .bUp = bUp,
    .u8BitIdx = 8,
    .u8OutHi = u8OutHi,
    .u8OutLo = u8OutLo
  };
  return sRet;
}

uint8_t bitsgen_next(SBitSGenState *psState) {
  if (psState->u8BitIdx == 8) {
    psState->u8Value = *psState->pu8Cur;
    psState->u8BitIdx = 0;
    ++psState->pu8Cur;
  }
  uint8_t u8Mask = 1 << (psState->bUp ? psState->u8BitIdx : (7 - psState->u8BitIdx));
  bool bBitHi = psState->u8Value & u8Mask;
  ++psState->u8BitIdx;
  return bBitHi ? psState->u8OutHi : psState->u8OutLo;
}

bool bitsgen_end(const SBitSGenState *psState) {
  return (psState->pu8Cur == psState->pu8End) && (psState->u8BitIdx == 8);
}

void bitsgen_reset(SBitSGenState *psState) {
  psState->pu8Cur = psState->pu8Begin;
  psState->u8BitIdx = 8;
}

SToByteFunctions gsBitSGenFunc = {
  .fNext = (FToByteNext)bitsgen_next,
  .fEnd = (FToXEnd)bitsgen_end,
  .fReset = (FToXReset)bitsgen_reset
};

bool test_bitgen_iter(SBitGenInitParam sParam, uint8_t u8Input, const uint8_t *pu8Exp) {
  SBitGenState sGen = bitgen_init(u8Input, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
  return gentest_check_seq8_equal(gsBitGenFunc, &sGen, pu8Exp, 8);
}

// after reset the stream must be continued from begin

bool test_bitgen_reset(SBitGenInitParam sParam, uint8_t u8Input, uint8_t u8ResetAt, const uint8_t *pu8Exp) {
  SBitGenState sGen = bitgen_init(u8Input, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
  uint8_t au8Actual[8];
  uint8_t i = 0;
  for (; i < u8ResetAt; ++i) {
    au8Actual[i] = bitgen_next(&sGen);
  }
  bitgen_resetv(&sGen, u8Input);
  for (; i < 8; ++i) {
    au8Actual[i] = bitgen_next(&sGen);
  }
  check_seq8_equal(au8Actual, pu8Exp, 8);
}

bool test_bitseqgen_iter(SBitGenInitParam sParam, const uint8_t *pu8Input, size_t szInputLen, const uint8_t *pu8Expected) {

  SBitGenState sBitGen = bitgen_init(0, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
  sBitGen.u8BitIdx = 8;
  SByteGenState sByteGen = bytegen_init(pu8Input, szInputLen);
  STrByteGenState sBitSeqGen = bitseqgen_init(&sByteGen, &sBitGen);
  return gentest_check_seq8_equal(gsBitSeqGenFunc, &sBitSeqGen, pu8Expected, 8 * szInputLen);
}

bool test_bitseqgen_reset(SBitGenInitParam sParam, const uint8_t *pu8Input, size_t szInputLen, const uint8_t *pu8Expected) {
  bool bRet = true;

  for (unsigned int i = 0; i < 8 * szInputLen; ++i) {
    SBitGenState sBitGen = bitgen_init(0, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
    sBitGen.u8BitIdx = 8;
    SByteGenState sByteGen = bytegen_init(pu8Input, szInputLen);
    STrByteGenState sBitSeqGen = bitseqgen_init(&sByteGen, &sBitGen);
    gentest_reset_after_steps(&sBitSeqGen, (FToByteNext) trbytegen_next, (FToXReset) trbytegen_reset, i);
    bRet &= gentest_check_seq8_equal(gsBitSeqGenFunc, &sBitSeqGen, pu8Expected, 8 * szInputLen);
  }
  return bRet;
}

bool test_bitsgen_iter(SBitGenInitParam sParam, const uint8_t *pu8Input, size_t szInputLen, const uint8_t *pu8Expected) {

  SBitSGenState sBitSGen = bitsgen_init(pu8Input, szInputLen, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
  return gentest_check_seq8_equal(gsBitSGenFunc, &sBitSGen, pu8Expected, 8 * szInputLen);
}

bool test_bitsgen_reset(SBitGenInitParam sParam, const uint8_t *pu8Input, size_t szInputLen, const uint8_t *pu8Expected) {
  bool bRet = true;

  for (unsigned int i = 0; i < 8 * szInputLen; ++i) {
    SBitSGenState sBitSGen = bitsgen_init(pu8Input, szInputLen, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
    gentest_reset_after_steps(&sBitSGen, (FToByteNext) bitsgen_next, (FToXReset) bitsgen_reset, i);
    bRet &= gentest_check_seq8_equal(gsBitSGenFunc, &sBitSGen, pu8Expected, 8 * szInputLen);
  }
  return bRet;
}

bool test_bitsgen_reset2(SBitGenInitParam sParam, const uint8_t *pu8Input, size_t szInputLen, const uint8_t *pu8Expected) {
  bool bRet = true;

  for (unsigned int i = 0; i < 8 * szInputLen; ++i) {
    SBitSGenState sBitSGen = bitsgen_init(pu8Input, szInputLen, sParam.bLsbFirst, sParam.u8High, sParam.u8Low);
    gentest_reset_after_steps(&sBitSGen, (FToByteNext) bitsgen_next, (FToXReset) bitsgen_reset, i);
    bRet &= gentest_check_seq8_equal(gsBitSGenFunc, &sBitSGen, pu8Expected, 8 * szInputLen);
  }
  return bRet;
}

bool test_bitpwmgen_iter(SBitPwmGenState *psBPGState, size_t szExpLen, uint16_t *pu16Exp) {
  bool bRet = true;
  for (size_t i = 0; i < szExpLen; ++i) {
    if (bitpwmgen_end(psBPGState)) {
      fprintf(stderr, "BitPwmGen premature end detected after output %zu\n", i);
      bRet = false;
      break;
    }
    uint16_t u16Val = bitpwmgen_next(psBPGState);
    if (pu16Exp[i] != u16Val) {
      fprintf(stderr, "BitPwmGen value mismatch at pos %zu (expected: %u, actual: %u)\n", i, pu16Exp[i], u16Val);
      bRet = false;
    }
  }
  if (!bitpwmgen_end(psBPGState)) {
    fprintf(stderr, "BitPwmGen end missing\n");
    bRet = false;
  }
  return bRet;
}

bool test_bitpwmgen_reset(SBitPwmGenState *psBPGState, uint32_t u32ResetAt, size_t szGenLen, uint16_t *pu16Exp) {
  bool bRet = true;

  size_t i = 0;
  for (; i < u32ResetAt; ++i) {
    bitpwmgen_next(psBPGState);
  }
  bitpwmgen_reset(psBPGState);

  for (; i < szGenLen; ++i) {
    uint16_t u16Val = bitpwmgen_next(psBPGState);
    if (pu16Exp[i] != u16Val) {
      fprintf(stderr, "test_bitpwmgen_reset() value mismatch at pos %zu (expected: %u, actual: %u)\n", i, pu16Exp[i], u16Val);
      bRet = false;
    }
  }
  return bRet;
}

uint8_t gau8Input2[1000];
uint8_t gau8Exp2[8000];

int main(int argc, char **argv) {

  if (1 < argc) {
    fprintf(stderr, "%s does not require command line arguments\n", argv[0]);
  }

  // bitgen tests
  assert(test_bitgen_iter(gasParam[0], gu8Input0, gau8Exp0_0));
  assert(test_bitgen_iter(gasParam[1], gu8Input0, gau8Exp0_1));
  assert(test_bitgen_iter(gasParam[2], gu8Input0, gau8Exp0_2));

  uint8_t au8BitgenResetExp[3][8] = {
    {1, 1, 1, 1, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 0, 0, 0},
    {1, 1, 1, 1, 0, 0, 0, 1}
  };
  assert(test_bitgen_reset(gasParam[0], 0xF0, 0, au8BitgenResetExp[0]));
  assert(test_bitgen_reset(gasParam[0], 0xF0, 1, au8BitgenResetExp[1]));
  assert(test_bitgen_reset(gasParam[0], 0xF0, 7, au8BitgenResetExp[2]));

  // bitseqgen tests
  assert(test_bitseqgen_iter(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));
  assert(test_bitseqgen_reset(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));

  assert(test_bitsgen_iter(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));
  assert(test_bitsgen_reset(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));

  memset(gau8Input2, 0xff, sizeof (gau8Input2));
  memset(gau8Exp2, 0x40, sizeof (gau8Exp2));
  assert(test_bitsgen_reset((SBitGenInitParam){false, 0x40, 0x01}, gau8Input2, ARRAY_SIZE(gau8Input2), gau8Exp2));


  // bitpwmgen tests
  uint8_t au8BitPwmGenInput[] = {0x00, 0xFF, 0xF0, 0x0F};
  SBitPwmGenState sBitPwmGenState = bitpwmgen_init(0x80, 0x00, 0x40, false, 0x2A, 0x10, ARRAY_SIZE(au8BitPwmGenInput), au8BitPwmGenInput);
  uint16_t au16BitPwmGenExp[] = {
    0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030,
    0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016,
    0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030,
    0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016
  };
  assert(test_bitpwmgen_iter(&sBitPwmGenState, 16 * ARRAY_SIZE(au8BitPwmGenInput), au16BitPwmGenExp));

  uint32_t u32BitPwmGen2ResetAt = 19;
  SBitPwmGenState sBitPwmGen2State = bitpwmgen_init(0x80, 0x00, 0x40, false, 0x2A, 0x10, ARRAY_SIZE(au8BitPwmGenInput), au8BitPwmGenInput);
  uint16_t au16BitPwmGen2Exp[] = {
    0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030,
    0x802A, 0x0016, 0x802A,
    0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030, 0x8010, 0x0030,
    0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016, 0x802A, 0x0016,
  };
  assert(test_bitpwmgen_reset(&sBitPwmGen2State, u32BitPwmGen2ResetAt, ARRAY_SIZE(au16BitPwmGen2Exp), au16BitPwmGen2Exp));


  return 0;
}