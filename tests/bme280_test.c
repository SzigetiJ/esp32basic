#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bme280.h"

bool test_duration_calc_1() {
  SBme280StateDesc sDesc = bme280_init_state();
  uint32_t au32Exp[] = {2, 3 + 4, 3 + 8, 3 + 16, 3 + 32, 3 + 64, 3 + 64, 3 + 64};
  bool bRet = true;
  for (uint8_t i = 0; i < 8; ++i) {
    sDesc.au8ConfigMirror[0] = i;
    uint32_t u32Res0 = bme280_measurement_duration_hms(&sDesc);
    if (au32Exp[i] != u32Res0) {
      fprintf(stderr, "Measurement duration calculation failed for osrs_h=%u: %u (exp: %u)\n", i, u32Res0, au32Exp[i]);
      bRet = false;
    }
  }
  return bRet;
}

bool test_duration_calc_2() {
  SBme280StateDesc sDesc = bme280_init_state();
  EBme280Osrs aeInput[][3] = {
    {0, 0, 1},  // H, P, T
    {0, 1, 0},
    {1, 0, 0},
    {1, 1, 1},
    {1, 2, 3}
  };
  uint32_t au32Exp[] = {
    2 + 4,
    2 + 5,
    2 + 5,
    2 + 5 + 5 + 4,
    2 + 5 + 9 + 16
  };
  bool bRet = true;
  for (uint8_t i = 0; i < 5; ++i) {
    bme280_set_osrs(&sDesc, BME280_SEL_H, aeInput[i][0]);
    bme280_set_osrs(&sDesc, BME280_SEL_P, aeInput[i][1]);
    bme280_set_osrs(&sDesc, BME280_SEL_T, aeInput[i][2]);
    memcpy(sDesc.au8ConfigMirror,sDesc.au8ConfigOut, 4);
    uint32_t u32Res = bme280_measurement_duration_hms(&sDesc);
    if (au32Exp[i] != u32Res) {
      fprintf(stderr, "Measurement duration calculation failed for input line #%u, osrs_hpt={%u,%u,%u}: {actual: %u, expected: %u}\n",
              i, aeInput[i][0], aeInput[i][1], aeInput[i][2],
              u32Res, au32Exp[i]);
      bRet = false;
    }
  }
  return bRet;
}

bool test_is_waiting() {
  SBme280StateDesc sDesc = bme280_init_state();
  bool bRet = true;
  bool bRes0 = bme280_is_waiting(&sDesc);
  if (bRes0) {
    fprintf(stderr, "Is waiting failed (expected: %d vs. actual: %d)\n", false, bRes0);
    bRet = false;
  }
  sDesc.u32CommState |= 0x00010000;
  bool bRes1 = bme280_is_waiting(&sDesc);
  if (!bRes1) {
    fprintf(stderr, "Is waiting failed (expected: %d vs. actual: %d)\n", true, bRes1);
    bRet = false;
  }
  return bRet;
}

int main() {
  assert(test_duration_calc_1());
  assert(test_duration_calc_2());
  assert(test_is_waiting());
  return 0;
}