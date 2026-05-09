#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "typeaux.h"
#include "1bme280/exptuner.h"

bool test_d10_step_inc_mul2() {
  bool bRet = true;
  SExpTuner2Desc sTunerD10 = exptuner2_init_b10d();
  SExpTuner2Value sLo = exptuner2d_lower_bound(&sTunerD10, 1);
  SExpTuner2Value sHi = exptuner2d_lower_bound(&sTunerD10, 55);
  SExpTuner2Value sVal = sLo;

  uint32_t au32Exp[] = {2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50,
  55, 55};
  for (size_t i = 0; i < ARRAY_SIZE(au32Exp); ++i) {
    exptuner2d_step(&sTunerD10, &sLo, &sHi, &sVal, true, true);
    uint32_t u32Actual = exptuner2d_get(&sTunerD10, &sVal);
    if (au32Exp[i] != u32Actual) {
      fprintf(stderr, "d10_step_inc_mul2 failed at step#%zu: %u (expected) vs. %u (actual)\n", i, au32Exp[i], u32Actual);
      bRet = false;
    }
  }
  return bRet;
}

bool test_d10_step_dec_mul2() {
  bool bRet = true;
  SExpTuner2Desc sTunerD10 = exptuner2_init_b10d();
  SExpTuner2Value sLo = exptuner2d_lower_bound(&sTunerD10, 1);
  SExpTuner2Value sHi = exptuner2d_lower_bound(&sTunerD10, 55);
  SExpTuner2Value sVal = sHi;

  uint32_t au32Exp[] = {50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20,
  19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1, 1};
  for (size_t i = 0; i < ARRAY_SIZE(au32Exp); ++i) {
    exptuner2d_step(&sTunerD10, &sLo, &sHi, &sVal, false, true);
    uint32_t u32Actual = exptuner2d_get(&sTunerD10, &sVal);
    if (au32Exp[i] != u32Actual) {
      fprintf(stderr, "d10_step_dec_mul2 failed at step#%zu: %u (expected) vs. %u (actual)\n", i, au32Exp[i], u32Actual);
      bRet = false;
    }
  }
  return bRet;
}

bool test_d10_step_inc_mul1() {
  bool bRet = true;
  SExpTuner2Desc sTunerD10 = exptuner2_init_b10d();
  SExpTuner2Value sLimitLo = exptuner2d_lower_bound(&sTunerD10, 1);
  SExpTuner2Value sLimitHi = exptuner2d_lower_bound(&sTunerD10, 550);
  SExpTuner2Value sVal = sLimitLo;

  uint32_t au32Exp[] = {2, 5, 10, 20, 50, 100, 200, 500, 550, 550};
  for (size_t i = 0; i < ARRAY_SIZE(au32Exp); ++i) {
    exptuner2d_step(&sTunerD10, &sLimitLo, &sLimitHi, &sVal, true, false);
    uint32_t u32Actual = exptuner2d_get(&sTunerD10, &sVal);
    if (au32Exp[i] != u32Actual) {
      fprintf(stderr, "d10_step_inc_mul1 failed at step#%zu: %u (expected) vs. %u (actual)\n", i, au32Exp[i], u32Actual);
      bRet = false;
    }
  }
  return bRet;
}

bool test_d10_step_dec_mul1() {
  bool bRet = true;
  SExpTuner2Desc sTunerD10 = exptuner2_init_b10d();
  SExpTuner2Value sLimitLo = exptuner2d_lower_bound(&sTunerD10, 1);
  SExpTuner2Value sLimitHi = exptuner2d_lower_bound(&sTunerD10, 600);
  SExpTuner2Value sVal = sLimitHi;

  uint32_t au32Exp[] = {500, 200, 100, 50, 20, 10, 5, 2, 1, 1};
  for (size_t i = 0; i < ARRAY_SIZE(au32Exp); ++i) {
    exptuner2d_step(&sTunerD10, &sLimitLo, &sLimitHi, &sVal, false, false);
    uint32_t u32Actual = exptuner2d_get(&sTunerD10, &sVal);
    if (au32Exp[i] != u32Actual) {
      fprintf(stderr, "d10_step_dec_mul1 failed at step#%zu: %u (expected) vs. %u (actual)\n", i, au32Exp[i], u32Actual);
      bRet = false;
    }
  }
  return bRet;
}

int main() {
  assert(test_d10_step_inc_mul1());
  assert(test_d10_step_dec_mul1());
  assert(test_d10_step_inc_mul2());
  assert(test_d10_step_dec_mul2());
  return 0;
}