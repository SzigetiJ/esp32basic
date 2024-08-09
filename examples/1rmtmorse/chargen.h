/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
/**
 * @file chargen.h
 * Header file of Character generator. Generators work just like C++ generators.
 * They have an internal state (here: S_CHARGEN_STATE) and there is a function
 * of type char (*)(_INTERNAL_STATE_*) (here: char chargen_next(S_CHARGEN_STATE*))
 * that serves a character everytime they get called, just like calling the operator()()
 * of a C++ generator.
 * 
 * This character generator produces the characters of a static character sequence.
 * After all the characters of the sequence are returned, there is a choice (bWrap):
 * either the sequence can be processed from the beginning again (bWrap = true),
 * or a preset character (cDone) is returned whenever the chargen_next() function is called (bWrap = false).
 * 
 * If the wrap-around feature is disabled, the user still can explicitly reset the iteration over the sequence
 * by calling chargen_reset().
 */
#ifndef CHARGEN_H
#define CHARGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

  typedef struct {
    const char *pcBegin;
    const char *pcEnd;
    const char *pcCur;
    bool bWrap; ///< wrap-around: if pcCur reaches pcEnd, it should jump to pcBegin.
    char cDone; ///< if wrap-around is disabled and pcCur reaches pcEnd, this character is generated.
  } SCharGenState;

  /**
   * Initializes a Character Generator state descriptor with wrap-around disabled.
   * @param pcText Underlying text to iterate on.
   * @param szTextLen Size of the text.
   * @param cDone Character to return after text end is reached.
   * @return Initialized state descriptor.
   */
  SCharGenState chargen_init_nowrap(const char *pcText, size_t szTextLen, char cDone);

  char chargen_next(SCharGenState *psState);
  bool chargen_end(const SCharGenState *psState);
  void chargen_reset(SCharGenState *psState);

#ifdef __cplusplus
}
#endif

#endif /* CHARGEN_H */

