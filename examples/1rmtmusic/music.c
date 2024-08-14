/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include "music.h"
#include "defines.h"

// per definition constants
#define MS_PER_10M       600000U       ///< millis per 10 minutes
// user-set constants
#define BP10M               720U       ///< Beats (quarter) per 10 minutes
#define FILL_STACCATO_CENT   25U
#define FILL_TENUTO_CENT     80U
#define FILL_LEGATO_CENT     95U

#define NOTEIDX_INVALID   ((NoteIdx)(-1))
// ==================== Local Constants ================

const uint8_t gau8ToneIdx[] = {
  9, 10, 0, 2, 4, 5, 7, 11
};  ///< Conversion table: EPitch => ToneIdx. EPitch base is 'A'.
const char *gastrToneBase[] = {
  "C.", "C#", "D.", "D#", "E.", "F.", "F#", "G.", "G#", "A.", "A#", "H."
};  ///< Conversion table: (ToneIdx % 12) => (EPitch, EModifier)

// Pitch period lengths
// 15289.025731886,
// 14430.918654256,
// 13620.973426152,
// 12856.486930665,
// 12134.907765182,
// 11453.827726317,
// 10810.973772752,
// 10204.200439176,
// 9631.482675994,
// 9090.909090909,
// 8580.675569834,
// 8099.079255821

/**
 * Rounded period length (µs) of every halftone in the base octave (where A is 110Hz).
 */
const uint32_t gau32tckBasePeriod[] = {
  15289,
  14431,
  13621,
  12856,
  12135,
  11454,
  10811,
  10204,
  9631,
  9091,
  8581,
  8099
};

// ==================== Implementation ================
bool note_is_pause(const SMusicNote *psNote) {
  return psNote->ePitch == PITCH_NONE;
}
NoteIdx note_get_idx(const SMusicNote *psNote) {
  if (psNote->ePitch < PITCH_A || PITCH_H < psNote->ePitch) return NOTEIDX_INVALID;
  if (psNote->eOctave < OCT_1 || OCT_7 < psNote->eOctave) return NOTEIDX_INVALID;
  NoteIdx u8Ret = gau8ToneIdx[psNote->ePitch - PITCH_A] + 12 * (psNote->eOctave - OCT_1);
  if (psNote->eModif == MODIF_FLAT) --u8Ret;
  if (psNote->eModif == MODIF_SHARP) ++u8Ret;
  return u8Ret;
};

SMusicNote *note_set_idx(SMusicNote *psNote, NoteIdx u8Idx) {
  psNote->eOctave = OCT_1 + u8Idx / 12;
  psNote->ePitch = gastrToneBase[u8Idx % 12][0];
  psNote->eModif = gastrToneBase[u8Idx % 12][1];
  return psNote;
}

SMusicNote *note_transpose(SMusicNote *psNote, int8_t i8Shift) {
  NoteIdx u8Idx = note_get_idx(psNote);
  if (u8Idx != NOTEIDX_INVALID) {
    u8Idx += i8Shift;
    note_set_idx(psNote, u8Idx);
  }
  return psNote;
}

WPeriod noteidx_to_wperiod(NoteIdx u8NoteIdx) {
  return (gau32tckBasePeriod[u8NoteIdx % 12]) >> (u8NoteIdx / 12);
}


uint32_t duration_as_divisor(EDuration eLen) {
  return eLen == LEN_WHOLE ? 1 :
          eLen == LEN_HALF ? 2 :
          eLen == LEN_QUARTER ? 4 :
          eLen == LEN_8 ? 8 :
          eLen == LEN_16 ? 16 :
          eLen == LEN_32 ? 32 :
          eLen == LEN_64 ? 64 :
          128;
}

uint32_t duration_ticks(EDuration eLen) {
  uint32_t u32BaseLen = (uint32_t) (4U * REFTICKS_PER_MS * MS_PER_10M) / (BP10M * RMT_DIVISOR);
  return u32BaseLen / duration_as_divisor(eLen);
}

uint32_t notelen_fill(uint32_t u32NoteLen, EMusicFill eFill) {
  return u32NoteLen * (
          eFill == FILL_CONT ? 100 :
          eFill == FILL_LEGATO ? FILL_LEGATO_CENT :
          eFill == FILL_STACCATO ? FILL_STACCATO_CENT :
          eFill == FILL_TENUTO ? FILL_TENUTO_CENT :
          50
          ) / 100;
}

uint32_t music_create_variation(SMusicNote *psDest, uint32_t u32DestSize, SMusicNote *psSrc, uint32_t u32SrcLen, int8_t *pi8Shift, uint8_t u8ShiftLen) {
  uint32_t u32DestIt = 0;
  for (uint32_t i = 0; i < u32SrcLen && u32DestIt < u32DestSize; ++i) {
    uint32_t u32LenDiv = duration_as_divisor(psSrc[i].eLen);
    uint32_t u32xNum = 64U / u32LenDiv;
    for (uint32_t j = 0; j < u32xNum && u32DestIt < u32DestSize; ++j) {
      SMusicNote sXNote = psSrc[i];
      if (!note_is_pause(&sXNote)) {
        note_transpose(&sXNote, pi8Shift[u32DestIt % u8ShiftLen]);
      }
      sXNote.eLen = LEN_64;
      sXNote.eFill = note_is_pause(&psSrc[i]) ? FILL_NONE : FILL_CONT;
      psDest[u32DestIt++] = sXNote;
    }
  }
  return u32DestIt;
}
