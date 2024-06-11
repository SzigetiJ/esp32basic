/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef ESP32TYPES_H
#define ESP32TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

  typedef volatile uint32_t Reg;
  typedef Reg *RegAddr;
  typedef void(*Isr)(void*);

  typedef enum {
    CPU_PRO = 0,
    CPU_APP = 1
  } ECpu;

  static inline void register_set(RegAddr prDst, Reg rValue) {
    *prDst = rValue;
  }

  static inline void register_set_bits(RegAddr prDst, Reg rValue, uint32_t u32Mask) {
    *prDst |= (rValue & u32Mask);
    *prDst &= (rValue & u32Mask) | ~u32Mask;
  }

  static inline Reg register_read(RegAddr prDst) {
    return *prDst;
  }

#ifdef __cplusplus
}
#endif

#endif /* ESP32TYPES_H */
