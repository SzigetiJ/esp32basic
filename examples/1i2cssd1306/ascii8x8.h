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

  // ============== Interface functions ==============
  bool ascii8x8_code_supported(uint8_t u8Code);
  void ascii8x8_get_tld(uint8_t *pu8Dest, uint8_t u8Code);
  void ascii8x8_get_blr(uint8_t *pu8Dest, uint8_t u8Code);

#ifdef __cplusplus
}
#endif

#endif /* ASCII8X8_H */

