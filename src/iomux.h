/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef IOMUX_H
#define IOMUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  /// IOMUX GPIO configuration register structure.

  typedef volatile union {

    struct {
      uint32_t u1McuOE : 1;
      uint32_t u1SlpSel : 1;
      uint32_t u1McuWPD : 1;
      uint32_t u1McuWPU : 1;
      uint32_t u1McuIE : 1;
      uint32_t u2McuDrv : 2;
      uint32_t u1FunWPD : 1;
      uint32_t u1FunWPU : 1;
      uint32_t u1FunIE : 1;
      uint32_t u2FunDrv : 2;
      uint32_t u3McuSel : 3;
      uint32_t rsvd15 : 17;
    };
    uint32_t raw;
  } IomuxGpioConfReg;

  /// Register address shift values for IOMUX GPIO configuration registers,
  /// where the base is grIOMUX. Shift is measured in sizeof(Reg).
  static const uint8_t gau8IomuxGpioIdx[] = {
    17, 34, 16, 33, 18,
    27, 24, 25, 26, 21,
    22, 23, 13, 14, 12,
    15, 19, 20, 28, 29,
    30, 31, 32, 35, 36,
    9, 10, 11, 0, 0,
    0, 0, 7, 8, 5,
    6, 1, 2, 3, 4
  };

  extern Reg grIOMUX; ///< IOMUX base register (IOMUX PIN CTRL)

  /// Sets IOMUX GPIO configuration register for a given GPIO.

  static inline void iomux_set_gpioconf(uint8_t u8GpioNum, IomuxGpioConfReg rGpioConf) {
    register_set(&grIOMUX + gau8IomuxGpioIdx[u8GpioNum], rGpioConf.raw);
  }

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */
