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

uint8_t au8Input2[1000];
uint8_t au8Exp2[8000];

int main(int argc, char **argv) {

  if (1 < argc) {
    fprintf(stderr, "%s does not require command line arguments\n", argv[0]);
  }

  assert(test_bitgen_iter(gasParam[0], gu8Input0, gau8Exp0_0));
  assert(test_bitgen_iter(gasParam[1], gu8Input0, gau8Exp0_1));
  assert(test_bitgen_iter(gasParam[2], gu8Input0, gau8Exp0_2));

//  assert(test_bitseqgen_iter(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));
//  assert(test_bitseqgen_reset(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));
  assert(test_bitsgen_iter(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));
  assert(test_bitsgen_reset(gsParam1, gau8Input1, ARRAY_SIZE(gau8Input1), gau8Expected1));

  memset(au8Input2, 0xff, sizeof (au8Input2));
  memset(au8Exp2, 0x40, sizeof (au8Exp2));
  assert(test_bitsgen_reset((SBitGenInitParam){false, 0x40, 0x01}, au8Input2, ARRAY_SIZE(au8Input2), au8Exp2));
  return 0;
}