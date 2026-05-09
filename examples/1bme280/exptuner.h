/*
 * Copyright 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef EXPTUNE_H
#define EXPTUNE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

  // ============== Defines ==============
  // ============== Types ==============

  typedef struct {
    uint8_t u8Base;
    uint8_t u8FinalDiv;
    uint8_t u8Mul1Size;
    uint8_t u8Mul2Size;
    const uint8_t *puMul1;
    const uint8_t *puMul2;
  } SExpTuner2Desc;

  typedef struct {
    uint32_t u32Base;
    uint8_t u8Mul1Idx;
    uint8_t u8Mul2Idx;
  } SExpTuner2Value;

  // ============== Global values / References ==============
  // ============== Interface functions ==============
  // -------------- Inline interface functions --------------
  // -------------- Interface functions --------------
  SExpTuner2Desc exptuner2_init_b10();
  SExpTuner2Value exptuner2_lower_bound(const SExpTuner2Desc *psTuner, uint32_t u32Value);
  bool exptuner2_step(const SExpTuner2Desc *psTuner, const SExpTuner2Value *psLowerBound, const SExpTuner2Value *psUpperBound, SExpTuner2Value *psValue, bool bUp, bool bSoft);
  uint32_t exptuner2_get(const SExpTuner2Desc *psTuner, const SExpTuner2Value * psValue);

  SExpTuner2Desc exptuner2_init_b10d();
  SExpTuner2Value exptuner2d_lower_bound(const SExpTuner2Desc *psTuner, uint32_t u32Value);
  bool exptuner2d_step(const SExpTuner2Desc *psTuner, const SExpTuner2Value *psLowerBound, const SExpTuner2Value *psUpperBound, SExpTuner2Value *psValue, bool bUp, bool bSoft);
  uint32_t exptuner2d_get(const SExpTuner2Desc *psTuner, const SExpTuner2Value * psValue);

#ifdef __cplusplus
}
#endif

#endif /* EXPTUNE_H */

