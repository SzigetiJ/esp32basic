/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef PRINT_H
#define PRINT_H

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

#define ZERO_CHR '0'
#define HEXA_LO_CHR 'a'

  static inline char *str_append(char *dst, const char *src) {
    return strcpy(dst, src) + strlen(src);
  }

  static inline char *print_dec_padded(char *dst, uint32_t u32Value, uint8_t u8Width, char cPad) {
    for (int i = 0; i < u8Width; ++i) {
      dst[u8Width - 1 - i] = u32Value ? ZERO_CHR + u32Value % 10U : cPad;
      u32Value /= 10U;
    }
    return dst + u8Width;
  }

  static inline char *print_dec_rev(char *dst, uint32_t num) {
    while (num) {
      char x = ZERO_CHR + (num % 10U);
      *dst++ = x;
      num /= 10;
    }
    return dst;
  }

  static inline char *print_dec(char *dst, uint32_t num) {
    char *dst_e = print_dec_rev(dst, num);
    for (int i = 0; dst + i < dst_e - 1 - i; ++i) {
      char tmp = *(dst_e - 1 - i);
      *(dst_e - 1 - i) = *(dst + i);
      *(dst + i) = tmp;
    }
    *dst_e = 0;
    return dst_e;
  }

  static inline char *print_deccent(char *dst, uint32_t u32Num, char cSep) {
    char *dst_e = print_dec(dst, u32Num / 100);
    if (dst_e == dst) {
      *(dst_e++) = ZERO_CHR;
    }
    *(dst_e++) = cSep;
    return print_dec_padded(dst_e, u32Num % 100, 2, ZERO_CHR);
  }

  static inline char *print_decmilli(char *dst, uint32_t u32Num, char cSep) {
    char *dst_e = print_dec(dst, u32Num / 1000);
    if (dst_e == dst) {
      *(dst_e++) = ZERO_CHR;
    }
    *(dst_e++) = cSep;
    return print_dec_padded(dst_e, u32Num % 1000, 3, ZERO_CHR);
  }

  static inline char hexdigit(uint8_t u8Value) {
    return (u8Value < 10 ? ZERO_CHR : HEXA_LO_CHR - 10) +u8Value;
  }

  static inline char *print_hex32(char *dst, uint32_t num) {
    for (int i = 0; i < 8; ++i) {
      dst[7 - i] = hexdigit(num & 0x0f);
      num >>= 4;
    }
    return dst + 8;
  }

  static inline char *print_hex16(char *dst, uint16_t num) {
    for (int i = 0; i < 4; ++i) {
      dst[3 - i] = hexdigit(num & 0x0f);
      num >>= 4;
    }
    return dst + 4;
  }

  static inline char *print_hex8(char *dst, uint8_t num) {
    for (int i = 0; i < 2; ++i) {
      dst[1 - i] = hexdigit(num & 0x0f);
      num >>= 4;
    }
    return dst + 2;
  }

#undef ZERO_CHR
#undef HEXA_LO_CHR

#ifdef __cplusplus
}
#endif

#endif /* PRINT_H */
