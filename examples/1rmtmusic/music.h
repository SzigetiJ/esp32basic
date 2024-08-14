/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef MUSIC_H
#define MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

  // ============= Types ===============

  typedef uint8_t NoteIdx; ///< (non-standard) Pitch index of a musical note.
  typedef uint32_t WPeriod; ///< Wave period (usually measured in ref_clk ticks).

  typedef enum {
    FILL_NONE = '.',
    FILL_LEGATO = 'l',
    FILL_STACCATO = 's',
    FILL_TENUTO = 't',
    FILL_CONT = 'c' ///< Continuous tone, no pause after the tone.
  } EMusicFill;

  typedef enum {
    PITCH_NONE = '.',
    PITCH_C = 'C',
    PITCH_D = 'D',
    PITCH_E = 'E',
    PITCH_F = 'F',
    PITCH_G = 'G',
    PITCH_A = 'A',
    PITCH_H = 'H'
  } EPitch;

  typedef enum {
    MODIF_FLAT = 'b',
    MODIF_SHARP = '#',
    MODIF_NONE = '.'
  } EPitchModifier;

  typedef enum {
    OCT_1 = '1',
    OCT_2 = '2',
    OCT_3 = '3',
    OCT_4 = '4',
    OCT_5 = '5',
    OCT_6 = '6',
    OCT_7 = '7',
    OCT_NONE = '.'
  } EOctave;

  typedef enum {
    LEN_WHOLE = '1',
    LEN_HALF = '2',
    LEN_QUARTER = '4',
    LEN_8 = '8',
    LEN_16 = 'X',
    LEN_32 = 'Y',
    LEN_64 = 'Z'
  } EDuration;

  typedef union {

    struct {
      EDuration eLen : 8;
      EPitch ePitch : 8;
      EPitchModifier eModif : 8;
      EOctave eOctave : 8;
      EMusicFill eFill : 8;
      char cEos; ///< Place for the terminating 0 character if the note is defined as C string.
    };
    char acRaw[6];
  } SMusicNote;

  bool note_is_pause(const SMusicNote *psNote);
  NoteIdx note_get_idx(const SMusicNote *psNote);
  SMusicNote *note_set_idx(SMusicNote *psNote, NoteIdx u8Idx);
  SMusicNote *note_transpose(SMusicNote *psNote, int8_t i8Shift);

  WPeriod noteidx_to_wperiod(NoteIdx NoteIdx);
  uint32_t duration_as_divisor(EDuration eLen);
  uint32_t duration_ticks(EDuration eLen);
  uint32_t notelen_fill(uint32_t u32NoteLen, EMusicFill eFill);

  uint32_t music_create_variation(SMusicNote *psDest, uint32_t u32DestSize, SMusicNote *psSrc, uint32_t u32SrcLen, int8_t *pi8Shift, uint8_t u8ShiftLen);


#ifdef __cplusplus
}
#endif

#endif /* MUSIC_H */

