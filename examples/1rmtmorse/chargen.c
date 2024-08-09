/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include "chargen.h"

SCharGenState chargen_init_nowrap(const char *pcText, size_t szTextLen, char cDone) {
  SCharGenState sRet = {
    .pcBegin = pcText,
    .pcCur = pcText,
    .pcEnd = pcText + szTextLen,
    .bWrap = false,
    .cDone = cDone
  };
  return sRet;
}

char chargen_next(SCharGenState *psState) {
  char cRet;
  if (chargen_end(psState)) {
    if (psState->bWrap) {
      psState->pcCur = psState->pcBegin;
      cRet = *psState->pcCur;
    } else {
      cRet = psState->cDone;
    }
  } else {
    cRet = *(psState->pcCur++);
  }
  return cRet;
}

void chargen_reset(SCharGenState *psState) {
  psState->pcCur = psState->pcBegin;
}

bool chargen_end(const SCharGenState *psState) {
  return psState->pcCur == psState->pcEnd;
}
