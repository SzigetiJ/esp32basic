/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdint.h>
#include "mphgen.h"

// ============== Defines ==============
#define CODE(X,Y) (((X)<<5) | ((Y)&0x1F))
#define CODE6(Y) (0xC0 | ((Y)&0x3F))
//============== Local types ==============

// ============== Local Data ==============
const uint8_t gau8AlphaCodeTable[] = {
  CODE(2, 01), CODE(4, 010), CODE(4, 012), CODE(3, 04),
  CODE(1, 0), CODE(4, 02), CODE(3, 06), CODE(4, 0),
  CODE(2, 0), CODE(4, 07), CODE(3, 05), CODE(4, 04),
  CODE(2, 03), CODE(2, 02), CODE(3, 07), CODE(4, 06),
  CODE(4, 015), CODE(3, 02), CODE(3, 0), CODE(1, 01),
  CODE(3, 01), CODE(4, 01), CODE(3, 03), CODE(4, 0101),
  CODE(4, 013), CODE(4, 014),
};
const uint8_t gau8NumCodeTable[] = {
  CODE(5, 037), CODE(5, 017), CODE(5, 07), CODE(5, 03), CODE(5, 01),
  CODE(5, 0), CODE(5, 020), CODE(5, 030), CODE(5, 034), CODE(5, 036)
};

const uint8_t gau8Sym0CodeTable[] = {// ASCII 32..47
  CODE(0, 0), CODE6(053), CODE6(022), CODE(0, 0), // SPC ! " #
  CODE6(004), CODE(0, 0), CODE(5, 010), CODE6(036), // $ % & '
  CODE(5, 026), CODE6(055), CODE(0, 0), CODE(5, 012), // ( ) * +
  CODE6(063), CODE6(041), CODE6(025), CODE(5, 022) // , - . /
};

const uint8_t gau8Sym1CodeTable[] = {// ASCII 58..64
  CODE6(070), CODE6(052), CODE(0, 0), CODE(5, 021), // : ; LT =
  CODE(0, 0), CODE6(014), CODE6(032)// > ? @
};
const uint8_t gau8Sym2CodeTable[] = {// ASCII 91..96
  CODE(0, 0), CODE(0, 0), CODE(0, 0), CODE(0, 0), // [ \\ ] ^
  CODE6(015), CODE(0, 0) // _ `
};
const uint8_t gu8CodeDefault = 0;

// ============== Local functions ===================
// -------------- declaration --------------------
static inline uint8_t _code_len(uint8_t u8Code);
static inline bool _code_val(uint8_t u8Code, uint8_t u8Pos);
static uint8_t _char_to_code(char cVal);
static inline void _load_next_char(SMphGenState *psState);
static inline EMorsePhase _ditdah_next(SMphGenState *psState);
// -------------- definition --------------------
/**
 * Derives the length of Morse-code from a 8 bit raw code value.
 * @param u8Code Raw code value.
 * @return Length of the Morse-code.
 */
static inline uint8_t _code_len(uint8_t u8Code) {
  uint8_t u8Ret = u8Code >> 5;
  if (u8Ret == 7) u8Ret = 6;
  return u8Ret;
}

/**
 * Tells the bit value of a Morse-code at a given position.
 * @param u8Code Raw code value.
 * @param u8Pos Bit position (0: last bit).
 * @return DAH (true) or DIT (false).
 */
static inline bool _code_val(uint8_t u8Code, uint8_t u8Pos) {
  return 0 < (u8Code & (1 << u8Pos));
}

/**
 * Encodes a text character into an 8 bit raw code.
 * @param cVal Text character to encode.
 * @return Encoded raw 8 bit value (gu8CodeDefault, if the text character cannot be encoded).
 */
static uint8_t _char_to_code(char cVal) {
  return ('A' <= cVal && cVal <= 'Z') ? (gau8AlphaCodeTable[cVal - 'A']) :
          ('a' <= cVal && cVal <= 'z') ? (gau8AlphaCodeTable[cVal - 'a']) :
          ('0' <= cVal && cVal <= '9') ? (gau8NumCodeTable[cVal - '0']) :
          (32 <= cVal && cVal <= 47) ? (gau8Sym0CodeTable[cVal - 32]) :
          (58 <= cVal && cVal <= 64) ? (gau8Sym1CodeTable[cVal - 58]) :
          (91 <= cVal && cVal <= 96) ? (gau8Sym2CodeTable[cVal - 91]) :
          gu8CodeDefault;
}

/**
 * Loads the next symbol into the internal state calling the underlying character generator and the encoder.
 * @param psState Internal state.
 */
static inline void _load_next_char(SMphGenState *psState) {
  psState->u8SymCur = _char_to_code(bytegen_next(psState->psChGen));
  psState->u8BitLen = _code_len(psState->u8SymCur);
  psState->u8BitIdx = 0;
}

/**
 * Tells the next bit of the symbol and increases the bit index.
 * @param psState State descriptor.
 * @return Bit value of the current symbol at the current position.
 */
static inline EMorsePhase _ditdah_next(SMphGenState *psState) {
  bool bDah = _code_val(psState->u8SymCur, _code_len(psState->u8SymCur) - psState->u8BitIdx - 1);
  ++psState->u8BitIdx;
  return bDah ? MORSE_DAH : MORSE_DIT;
}

// ============== Interface functions ==============

SMphGenState mphgen_init(SByteGenState *psChGen, bool bWithSSpace) {
  SMphGenState sRet = {.psChGen = psChGen, .u8SymCur = 0, .u8BitIdx = 0, .eLastPhase = MORSE_NOP, .bWithSSpace = bWithSSpace};
  return sRet;
}

EMorsePhase mphgen_next(SMphGenState *psState) {
  // When to load next char? there are 3 cases:
  // 1.) state is MPH_START;
  // 2.) last output was a WSPACE (state is MPH_WSPACE);
  // 3.) last output was DIT or DAH (state is MPH_DITDAH) AND bitidx reached bitlen.
  // In each case (BitIdx == BitLen) is true.
  // When NOT to load next char?
  // ...
  if (psState->u8BitIdx == psState->u8BitLen) {
    _load_next_char(psState);
  }
  // what to output?
  // a.) If current symbol is SPACE, output WSPACE
  // b.) If new symbol (not SPACE) was loaded AND state is MPH_DITDAH, output LSPACE
  // c.) If within symbol AND state is MPH_DITDAH AND bwithSSpace, output SSPACE
  // d1.) If within symbol AND state is MPH_SSPACE OR MPH_WSPACE OR MPH_LSPACE, output DITDAH (++u8BitIdx)
  // d2.) If within symbol AND state is MPH_DITDAH AND NOT bWithSSpace, output DITDAH (++u8BitIdx)
  if (psState->u8BitLen == 0) { // symbol is SPACE
    psState->eLastPhase = MORSE_WSPACE;
  } else if (psState->u8BitIdx == 0 && psState->eLastPhase <= MORSE_DAH) { // new symbol is loaded after DITDAH
    psState->eLastPhase = MORSE_LSPACE;
  } else if (psState->bWithSSpace && psState->eLastPhase <= MORSE_DAH) {
    psState->eLastPhase = MORSE_SSPACE;
  } else {
    psState->eLastPhase = _ditdah_next(psState);
  }
  return psState->eLastPhase;
}

void mphgen_reset(SMphGenState *psMphGen) {
  bytegen_reset(psMphGen->psChGen);
  psMphGen->u8SymCur = 0;
  psMphGen->u8BitLen = 0;
  psMphGen->u8BitIdx = 0;
  psMphGen->eLastPhase = MORSE_NOP;
}

bool mphgen_end(const SMphGenState *psState) {
  return bytegen_end(psState->psChGen) &&
          (psState->u8BitIdx == psState->u8BitLen);
}
