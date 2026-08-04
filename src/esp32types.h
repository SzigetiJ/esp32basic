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

  // ============== Defines ==============
/// Transforms number of bits into bitmask, e.g., BITS2MASK(3): 7, BITS2MASK(5): 31
#define BITS2MASK(X) ( ( 1 << ( X ) ) - 1)
/// Get a field (shifted to LSB) form a (register) value, e.g., with FOO_BITS=3 and FOO_OFS=2, FIELD_GET(0x2C, FOO): 3 (bits2..4)
#define FIELD_GET(VAL,FIELD) ( ( (VAL) >> FIELD##_OFS ) & BITS2MASK( FIELD##_BITS ) )
/// Get the bitmask of a given field.
#define FIELD_MASK(PFX) ( BITS2MASK( PFX##_BITS ) << PFX##_OFS )
/// Take a field value (X), first crop it to the given bitsize, then shift it to its position (offset).
#define FIELD_MASKNSHIFT(X,PFX) ( ( X ) & BITS2MASK( PFX##_BITS ) << PFX##_OFS )
/// Take a register value (VAL), and replace its field (PFX) with a given value (X).
#define FIELD_REPLACE(VAL,X,PFX) ( ( ( VAL ) & ~( BITS2MASK( PFX##_BITS ) << PFX##_OFS ) ) | ( ( ( X ) & BITS2MASK( PFX##_BITS )) << PFX##_OFS ) )

  // ============== Types ==============

  typedef volatile uint32_t Reg;
  typedef Reg *RegAddr;
  typedef void(*Isr)(void*);

  typedef enum {
    CPU_PRO = 0,
    CPU_APP = 1
  } ECpu;

  // ============== Global values / References ==============

  // ============== Inline interface functions ==============
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

  // ============== Interface functions ==============

#ifdef __cplusplus
}
#endif

#endif /* ESP32TYPES_H */
