/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef TYPEAUX_H
#define TYPEAUX_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(X[0]))

  static inline uint16_t conv16be(uint16_t X) {
    return (X >> 8) | ((X & 0xFF) << 8);
  }

#ifdef __cplusplus
}
#endif

#endif /* TYPEAUX_H */
