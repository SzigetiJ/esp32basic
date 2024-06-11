/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef XTUTILS_H
#define XTUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_attr.h"
#define XCHAL_HAVE_S32C1I 1
#define SOC_CPU_CORES_NUM 2

    /* Taken from https://github.com/espressif/esp-idf/blob/master/components/xtensa/include/xt_utils.h
   * which is released under SPDX-License-Identifier: Apache-2.0 */
  FORCE_INLINE_ATTR bool xt_utils_compare_and_set(volatile uint32_t *addr, uint32_t compare_value, uint32_t new_value) {
#if XCHAL_HAVE_S32C1I
#ifdef __clang_analyzer__
    //Teach clang-tidy that "addr" cannot be const as it can be updated by S32C1I instruction
    volatile uint32_t temp;
    temp = *addr;
    *addr = temp;
#endif
    // Atomic compare and set using S32C1I instruction
    uint32_t old_value = new_value;
    __asm__ __volatile__ (
            "WSR    %2, SCOMPARE1 \n"
            "S32C1I %0, %1, 0 \n"
            : "=r"(old_value)
            : "r"(addr), "r"(compare_value), "0"(old_value)
            );

    return (old_value == compare_value);
#else // XCHAL_HAVE_S32C1I
    // Single core target has no atomic CAS instruction. We can achieve atomicity by disabling interrupts
    uint32_t intr_level;
    __asm__ __volatile__ ("rsil %0, " XTSTR(XCHAL_EXCM_LEVEL) "\n"
            : "=r"(intr_level));
    // Compare and set
    uint32_t old_value;
    old_value = *addr;
    if (old_value == compare_value) {
      *addr = new_value;
    }
    // Restore interrupts
    __asm__ __volatile__ ("memw \n"
            "wsr %0, ps\n"
            ::"r"(intr_level));

    return (old_value == compare_value);
#endif // XCHAL_HAVE_S32C1I
  }

    /* Taken from https://github.com/espressif/esp-idf/blob/master/components/xtensa/include/xt_utils.h
   * which is released under SPDX-License-Identifier: Apache-2.0 */
  FORCE_INLINE_ATTR __attribute__ ((pure)) uint32_t xt_utils_get_core_id(void) {
    /*
    Note: We depend on SOC_CPU_CORES_NUM instead of XCHAL_HAVE_PRID as some single Xtensa targets (such as ESP32-S2) have
    the PRID register even though they are single core.
     */
#if SOC_CPU_CORES_NUM > 1
    // Read and extract bit 13 of special register PRID
    uint32_t id;
    __asm__ __volatile__ (
            "rsr.prid %0\n"
            "extui %0,%0,13,1"
            : "=r"(id));
    return id;
#else
    return 0;
#endif // SOC_CPU_CORES_NUM > 1
  }


#ifdef __cplusplus
}
#endif

#endif /* XTUTILS_H */
