#ifndef GENTEST_COMMON_H
#define GENTEST_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/generators.h"

  static inline void gentest_reset_after_steps(void *pvState, FToByteNext fNext, FToXReset fReset, int iCycles) {
    for (int i = 0; i < iCycles; ++i) {
      fNext(pvState);
    }
    fReset(pvState);
  }

  bool check_seq8_equal(const uint8_t *pu8Act, const uint8_t *pu8Exp, size_t szLen);
  bool gentest_check_seq8_equal(const SToByteFunctions sFunc, void *pvSeqGen, const uint8_t *pu8Exp, size_t szExpLen);
  bool gentest_check_seq16_equal(const SToWordFunctions sFunc, void *pvSeqGen, const uint16_t *pu16Exp, size_t szExpLen);



#ifdef __cplusplus
}
#endif

#endif /* GENTEST_COMMON_H */

