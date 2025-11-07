/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef I2CIFACE_H
#define I2CIFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c.h"
#include "lockmgr.h"

  // ============== Defines ==============

  // ============== Types ==============
  /**
   * Interface information how to access I2C slave device.
   */
  typedef struct {
    EI2CBus eBus;
    uint8_t u8SlaveAddr;
    ELockmgrResource eLck;
  } SI2cIfaceCfg;

  // ============== Global values / References ==============

  // ============== Inline interface functions ==============

  // ============== Interface functions ==============
#ifdef __cplusplus
}
#endif

#endif /* I2CIFACE_H */

