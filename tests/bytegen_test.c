#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "utils/generators.h"
#include "gentest_common.h"
#include "typeaux.h"

const char gacInput0[] = "abc def";

uint8_t gau8Input1[] = {3, 5, 7, 9};
uint8_t gau8Param1[] = {10, 0x80, 0x00};
uint16_t gau16Expected1[] = {0x8003, 0x0007, 0x8005, 0x0005, 0x8007, 0x0003, 0x8009, 0x0001};

bool test_bytegen_iter(const char *str) {
  SByteGenState sGen = bytegen_init((uint8_t*) str, strlen(str));
  return gentest_check_seq8_equal(gsByteGenFunc, (void*) &sGen, (uint8_t*) gacInput0, strlen(gacInput0));
}

bool test_bytegen_reset(const char *str) {
  bool bRet = true;
  for (unsigned int i = 0; i < strlen(str); ++i) {
    SByteGenState sGen = bytegen_init((uint8_t*) str, strlen(str));
    gentest_reset_after_steps(&sGen, (FToByteNext) bytegen_next, (FToXReset) bytegen_reset, i);
    bRet &= gentest_check_seq8_equal(gsByteGenFunc, (void*) &sGen, (uint8_t*) gacInput0, strlen(gacInput0));
  }
  return bRet;
}

bool test_pwngen_iter(const uint8_t *pu8Data, size_t szDataLen, const uint16_t *pu16Expected, uint8_t u8PLen, uint8_t u8HiUpper, uint8_t u8LoUpper) {
  SByteGenState sByteGen = bytegen_init(pu8Data, szDataLen);
  SPwmGenState sPWM = pwmgen_init((void*) &sByteGen, &gsByteGenFunc, u8PLen, u8HiUpper, u8LoUpper);
  return gentest_check_seq16_equal(gsPwmGenFunc, (void*) &sPWM, pu16Expected, 2 * szDataLen);
}

bool test_pwmgen_reset(const uint8_t *pu8Data, size_t szDataLen, const uint16_t *pu16Expected, uint8_t u8PLen, uint8_t u8HiUpper, uint8_t u8LoUpper) {
  bool bRet = true;
  for (unsigned int i = 0; i < 2 * szDataLen; ++i) {
    SByteGenState sByteGen = bytegen_init(pu8Data, szDataLen);
    SPwmGenState sPWM = pwmgen_init((void*) &sByteGen, &gsByteGenFunc, u8PLen, u8HiUpper, u8LoUpper);
    gentest_reset_after_steps(&sPWM, (FToByteNext) pwmgen_next, (FToXReset) pwmgen_reset, i);
    bRet &= gentest_check_seq16_equal(gsPwmGenFunc, (void*) &sPWM, pu16Expected, 2 * szDataLen);
  }
  return bRet;
}

int main(int argc, char **argv) {

  if (1 < argc) {
    fprintf(stderr, "%s does not require command line arguments\n", argv[0]);
  }

  assert(test_bytegen_iter(gacInput0));
  assert(test_bytegen_reset(gacInput0));
  assert(test_pwngen_iter(gau8Input1, ARRAY_SIZE(gau8Input1), gau16Expected1, gau8Param1[0], gau8Param1[1], gau8Param1[2]));
  assert(test_pwmgen_reset(gau8Input1, ARRAY_SIZE(gau8Input1), gau16Expected1, gau8Param1[0], gau8Param1[1], gau8Param1[2]));
}