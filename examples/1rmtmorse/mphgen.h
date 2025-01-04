/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
/** @file mphgen.h
 * Morse Phase Generator.
 * A Morse-encoded text is a sequence of different symbols (long or short signs)
 * and inter-symbol spaces (space within a letter, space between letters, longer space representing a textual space).
 * Here we call all these symbols and spaces Morse-phases.
 * The Morse Phase Generator takes a text generator and outputs a morse phase sequence according to the output of the text generator.
 * The ~reset function also resets the text generator, and the ~end function also takes into account the state of the text generator.
 */
#ifndef MPHGEN_H
#define MPHGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "utils/generators.h"

  typedef enum {
    MORSE_DIT = 0,
    MORSE_DAH,
    MORSE_SSPACE, ///< Space within a letter
    MORSE_LSPACE, ///< Space separating letters
    MORSE_WSPACE, ///< Space representing a letter without a Morse-code
    MORSE_NOP ///< Non of the former phases.
  } EMorsePhase;

  typedef struct {
    SByteGenState *psChGen; ///< Internal state of the source / input generator.
    uint8_t u8SymCur; ///< Current symbol.
    uint8_t u8BitLen; ///< Bitlength of current symbol.
    uint8_t u8BitIdx; ///< Bit position of phase within the current symbol.
    EMorsePhase eLastPhase; ///< Last output phase.
    bool bWithSSpace; ///< Config: put space between neighboring DIT or DAH phases.
  } SMphGenState;

  /**
   * Initializes the state descriptor.
   * @param psChGen State descriptor of the underlying character generator.
   * @param bWithSSpace Initial value of bWithSSpace configuration attribute.
   * @return Initialized state descriptor.
   */
  SMphGenState mphgen_init(SByteGenState *psChGen, bool bWithSSpace);

  /**
   * Generates next Morse phase.
   * @param psState State descriptor.
   * @return Next Morse phase.
   */
  EMorsePhase mphgen_next(SMphGenState *psState);

  /**
   * Indicates wether the generator has output the last phase of the source text.
   * @param psState State descriptor.
   * @return End of the phase sequence.
   */
  bool mphgen_end(const SMphGenState *psState);

  /**
   * Resets the generator.
   * @param psMphGen State descriptor.
   */
  void mphgen_reset(SMphGenState *psMphGen);

#ifdef __cplusplus
}
#endif

#endif /* MPHGEN_H */

