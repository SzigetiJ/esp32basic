/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
/** @file bitgen.h
 * Bitphase Generator for WS2812.
 * WS2812 byte encoding is the following:
 *  * The bits are transferred in MSB first, LSB last order.
 *  * Each bit is encoded into two phases: it starts in high phase and ends in low phase.
 * As a result, there will be generated 2 * 8 = 16 phases (see EBitPhase) for each byte.
 */
#ifndef MPHGEN_H
#define MPHGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

  typedef enum {
    BIT_H0 = 0,
    BIT_L0,
    BIT_H1,
    BIT_L1,
    BIT_END
  } EBitPhase;

  typedef struct {
    const uint8_t *pu8Begin;  ///< Begin of the byte array subject to transmit.
    const uint8_t *pu8End;    ///< End of the byte array subject to transmit.
    const uint8_t *pu8Cur;    ///< Cursor to the byte being transmitted.
    uint8_t u8BitIdx;         ///< Bit transmit counter within the current symbol. 0: bit7 is being transmitted, [...] 7: bit0 is being transmitted.
    EBitPhase eLastPhase;     ///< Last transmitted phase.
  } SBitGenState;

  /**
   * Checks if phase is low (L0 or L1).
   * @param ePhase Phase to check.
   * @return true: L0 or L1, false: other.
   */
  bool bitgen_is_low(EBitPhase ePhase);

  /**
   * Initializes the state descriptor.
   * @param pcBegin Pointer to byte sequence.
   * @param u32Size Length of the byte sequence.
   * @return Initialized state descriptor.
   */
  SBitGenState bitgen_init(const uint8_t *pu8Begin, uint32_t u32Size);

  /**
   * Generates next Morse phase.
   * @param psState State descriptor.
   * @return Next Morse phase.
   */
  EBitPhase bitgen_next(SBitGenState *psState);

  /**
   * Indicates wether the generator has output the last phase of the source text.
   * @param psState State descriptor.
   * @return End of the phase sequence.
   */
  bool bitgen_end(const SBitGenState *psState);

  /**
   * Resets the generator.
   * @param psState State descriptor.
   */
  void bitgen_reset(SBitGenState *psState);

#ifdef __cplusplus
}
#endif

#endif /* MPHGEN_H */

