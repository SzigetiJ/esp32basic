/*
 * Copyright 2024 - 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef CRC_H
#define CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

  uint8_t crc8byte(uint8_t u8Poly, uint8_t u8Input);
  uint8_t crc8(uint8_t u8Poly, uint8_t u8Init, const uint8_t *pu8Message, size_t szMessageLen);

#ifdef __cplusplus
}
#endif

#endif /* CRC_H */

