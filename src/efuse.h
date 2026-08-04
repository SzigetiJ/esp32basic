/*
 * Copyright 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef EFUSE_H
#define EFUSE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "esp32types.h"

  // ============== Defines ==============
#define EFUSE_VOL_LVL_HP_INV 0  // use as PFX
#define EFUSE_VOL_LVL_HP_INV_BLK0REG 5U
#define EFUSE_VOL_LVL_HP_INV_OFS 22U
#define EFUSE_VOL_LVL_HP_INV_BITS 2U

#define EFUSE_DIG_VOL_L6 0  // use as PFX
#define EFUSE_DIG_VOL_L6_BLK0REG 5U
#define EFUSE_DIG_VOL_L6_OFS 24U
#define EFUSE_DIG_VOL_L6_BITS 4

  // ============== Types ==============
  typedef struct {
    Reg BLK0RDATA[7];
    Reg rsvd_x20[5];
    Reg BLK1RDATA[8];
    Reg BLK2RDATA[8];
    Reg BLK3RDATA[8];
    Reg rsvd_x98[0x60];
    Reg CLK;
    Reg CONF;
    Reg CMD;
    Reg INT_RAW;
    Reg INT_ST;
    Reg INT_ENA;
    Reg INT_CLR;
    Reg DAC_CONF;
    Reg DEC_STATUS;
  } EFUSE_Type;

  // ============== Global values / References ==============
  extern EFUSE_Type gsEFUSE;

  // ============== Inline interface functions ==============

  // ============== Interface functions ==============





#ifdef __cplusplus
}
#endif

#endif /* EFUSE_H */

