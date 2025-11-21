/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef ASCII8X8_H
#define ASCII8X8_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

  // ============== Defines ==============

  // ============== Types ==============

  typedef enum {
    ASCII_CHRSET_8x8 = 0,
    ASCII_CHRSET_8x6,
    ASCII_CHRSET_6x4
  } EAsciiCharset;

  typedef struct {
    uint8_t u8Height;
    uint8_t u8Width;
    bool bBold;
  } SAsciiAttributes;

  // ============== Interface functions ==============
  uint32_t ascii_supported_charsets();
  SAsciiAttributes ascii_charset_attr(EAsciiCharset eCharset);

  bool ascii_code_supported(EAsciiCharset eCharset, uint8_t u8Code);
  uint8_t ascii_get_tld(uint8_t *pu8Dest, EAsciiCharset eCharset, uint8_t u8Code);
  void ascii8x8_get_blr(uint8_t *pu8Dest, uint8_t u8Code);

#ifdef __cplusplus
}
#endif

#endif /* ASCII8X8_H */

