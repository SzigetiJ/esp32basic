/*
 * Copyright 2024 - 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include "crc.h"

/**
 * Calculates CRC8 value of a single byte.
 * @param u8Poly CRC polynomial.
 * @param u8Input Input value.
 * @return CRC8 of u8Input.
 */
uint8_t crc8byte(uint8_t u8Poly, uint8_t u8Input) {
  static const uint32_t u32PadMask = 0xFF;
  uint32_t u32Input = u8Input << 8;
  uint32_t u32Divisor = u8Poly | 0x100;
  while (u32Divisor < u32Input) u32Divisor <<= 1;
  while (u32PadMask < u32Input) {
    if ((u32Input ^ u32Divisor) < u32Input) {
      u32Input ^= u32Divisor;
    }
    u32Divisor >>= 1;
  }
  return (uint8_t) u32Input;
}

/**
 * Calculates the CRC8 value of a bytestream.
 * @param u8Poly CRC polynomial.
 * @param u8Init Inital CRC value
 * @param pu8Message Input bytestream.
 * @param szMessageLen Length of the input.
 * @return CRC8 of the bytestream.
 */
uint8_t crc8(uint8_t u8Poly, uint8_t u8Init, const uint8_t *pu8Message, size_t szMessageLen) {
  uint8_t u8Crc = u8Init;
  for (size_t i = 0; i < szMessageLen; ++i) {
    u8Crc = crc8byte(u8Poly, u8Crc ^ pu8Message[i]);
  }
  return u8Crc;
}
