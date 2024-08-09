/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef RMTUTILS_H
#define RMTUTILS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "rmt.h"

  // ============== Types ==============
  typedef uint16_t(*U16Generator)(void *pvParam);
  typedef bool(*UniRel)(void *pvParam);

  /**
   * State descriptor of Stretch Generator.
   * With stretch generator the user can lengthen RMT periods by N/M times.
   * It takes an entry generator, a multiplier / numerator (N) and a divisor / denominator (M);
   * and internally it maintains the entry output level and the remaining ouput period.
   */
  typedef struct {
    U16Generator fGen; ///< Underlying entry generator.
    UniRel fGenEnd; ///< Sequence end indicator function for the underlying generator.
    void *pvGenParam; ///< State descriptor of the underlying generator.
    uint32_t u32Multiplier; ///< M value of the M/N Period length multiplier.
    uint32_t u32Divisor; ///< N value of the M/N Period length multiplier.
    uint32_t u32OutQueue; ///< Remaining period length (a single input entry may result in multiple output entries).
    bool bLevel; ///< Current output entry signal level.
  } SStretchGenState;

  // ============== Interface functions ==============

  SStretchGenState rmtutils_init_stretchgenstate(uint32_t u32Multiplier, uint32_t u32Divisor, U16Generator fGen,
          UniRel fGenEnd, void *pvGenParam);

  bool rmtutils_feed_tx_stretched(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, SStretchGenState *psSGenState);

  bool rmtutils_feed_tx(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, U16Generator pfGen, UniRel pfEnd, void *pvGen);




#ifdef __cplusplus
}
#endif

#endif /* RMTUTILS_H */

