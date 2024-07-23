/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef I2CUTILS_H
#define I2CUTILS_H

#include <stdint.h>
#include "i2ciface.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct {
    uint32_t u32LastLabel;
    uint8_t au8Slave[16];
    uint8_t u8SlaveAddr;
    bool bWaitingForI2c;
  } SI2cScanStateDesc;

  /**
   * Initializes an SI2cScanStateDesc object.
   * @return Clean SI2cScanStateDesc object.
   */
  SI2cScanStateDesc i2cutil_scan_init();

  /**
   * Scans the whole I2C slave address space for devices (replying with ACK to the first I2C message).
   * This function implements both the RX and TX part of the asynchronous comm. process.
   * To perform the whole interval scan, this function must be called repeatedly
   * (after _i2c_release_cycle() calls) as long as it returns false.
   * @param psIface Information for accessing I2C bus and LockManager. The slave address is NOT used.
   * @param psState State descriptor to update.
   * @return Scan complete.
   */
  bool i2cutils_scan_cycle(const SI2cIfaceCfg *psIface, SI2cScanStateDesc *psState);

#ifdef __cplusplus
}
#endif

#endif /* I2CUTILS_H */

