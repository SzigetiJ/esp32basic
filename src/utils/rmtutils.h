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
  /**
   * Generalized generator function generating uint16_t values.
   * In C++, generators are functors implementing T operator()(), i.e.,
   * parameterless function return type T.
   * 
   * Here, in standard C environment, we have to pass the state descriptor
   * of the generator (pvParam) to the generator function.
   */
  typedef uint16_t(*U16Generator)(void *pvParam);
  /**
   * Generalized unary relation, taking a single argument.
   */
  typedef bool(*UniRel)(const void *pvParam);

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

  /**
   * Adapter function to rmtutils_feed_tx().
   * The feeder will not take directly the values from the U16Generator, but via a Stretch generator.
   * @param eChannel
   * @param pu16MemPos
   * @param u16Len
   * @param psSGenState
   * @return 
   */
  bool rmtutils_feed_tx_stretched(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, SStretchGenState *psSGenState);

  /**
   * RMT RAM block feeder function, taking RMT entries from pfGen.
   * @param eChannel Identifies the RMT channel.
   * @param pu16MemPos Input/output parameter, pointer to the memory offset value.
   * In: where to start writing the memory, out: here can you continue writing the memory.
   * @param u16Len Amount of registers(!) (pair of RMT entries) to write.
   * @param pfGen Underlying generator to take the RMT entries from.
   * @param pfEnd End of sequence indicator function of the underlying generator.
   * @param pvGen Generator state descriptor to pass to pfGen and pfEnd.
   * @return Terminating entry has been written.
   */
  bool rmtutils_feed_tx(ERmtChannel eChannel, uint16_t *pu16MemPos, uint16_t u16Len, U16Generator pfGen, UniRel pfEnd, void *pvGen);

#ifdef __cplusplus
}
#endif

#endif /* RMTUTILS_H */

