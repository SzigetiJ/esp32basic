
#include <stdio.h>
#include <stdbool.h>
#include "gentest_common.h"

bool gentest_check_seq8_equal(const SToByteFunctions sFunc, void *pvSeqGen, const uint8_t *pu8Exp, size_t szExpLen) {
  bool bRet = true;
  for (size_t i = 0; i < szExpLen; ++i) {
    bool bEnd = sFunc.fEnd(pvSeqGen);
    uint8_t u8Val = sFunc.fNext(pvSeqGen);
    if (bEnd) {
      fprintf(stderr, "generator marks end too early: %zu vs. %zu\n", i, szExpLen);
      bRet = false;
    }
    if (u8Val != pu8Exp[i]) {
      fprintf(stderr, "bit mismatch at position #%zu: %u vs. %u\n", i, u8Val, pu8Exp[i]);
      bRet = false;
    }
  }
  bool bEndOk = sFunc.fEnd(pvSeqGen);
  if (!bEndOk) {
    fprintf(stderr, "generator marks end too late: %zu\n", szExpLen);
    bRet = false;
  }
  return bRet;
}

bool gentest_check_seq16_equal(const SToWordFunctions sFunc, void *pvSeqGen, const uint16_t *pu16Exp, size_t szExpLen) {
  bool bRet = true;
  for (size_t i = 0; i < szExpLen; ++i) {
    bool bEnd = sFunc.fEnd(pvSeqGen);
    uint16_t u16Val = sFunc.fNext(pvSeqGen);
    if (bEnd) {
      fprintf(stderr, "generator marks end too early: %zu vs. %zu\n", i, szExpLen);
      bRet = false;
    }
    if (u16Val != pu16Exp[i]) {
      fprintf(stderr, "bit mismatch at position #%zu: %u vs. %u\n", i, u16Val, pu16Exp[i]);
      bRet = false;
    }
  }
  bool bEndOk = sFunc.fEnd(pvSeqGen);
  if (!bEndOk) {
    fprintf(stderr, "generator marks end too late: %zu\n", szExpLen);
    bRet = false;
  }
  return bRet;
}

