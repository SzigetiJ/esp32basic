/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef PIDCTRL_H
#define PIDCTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  typedef struct {
    Reg INT_EN; // bits [1..7]
    Reg INT_ADDR[7]; //
    Reg PID_DELAY; // bits [0..11]
    Reg NMI_DELAY; // bits [0..11]
    Reg LEVEL; // bits[0..3]
    Reg FROM[7]; // bits [0..6]: [0..2]: prev.proc, [3..6]: prev.intr
    Reg PID_NEW; // bits [0..2]
    Reg PID_CONFIRM; // bit #0
    Reg rsvd0;
    Reg NMI_MASK_EN; // bit #0
    Reg NMI_MASK_DIS; // bit #0
  } PIDCTRL_Type;

  extern PIDCTRL_Type gsPIDCTRL;

#ifdef __cplusplus
}
#endif

#endif /* PIDCTRL_H */
