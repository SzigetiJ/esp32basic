/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <string.h>
#include <stdbool.h>

#include "i2cutils.h"
#include "i2c.h"

SI2cScanStateDesc i2cutil_scan_init() {
  SI2cScanStateDesc sRet;
  memset(&sRet, 0, sizeof(sRet));
  sRet.u8SlaveAddr = -1;
  return sRet;
}

bool i2cutils_scan_cycle(const SI2cIfaceCfg *psIface, SI2cScanStateDesc *psState) {
  // check_result phase (RX)
  if (psState->bWaitingForI2c) {
    AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
    if (psEntry) {
      if (psEntry->bReady) {
        if (0 == (psEntry->u32IntSt & I2C_INT_MASK_ERR)) {
          psState->au8Slave[psState->u8SlaveAddr/8] |= 1 << (psState->u8SlaveAddr%8);
        }
        lockmgr_release_entry(psState->u32LastLabel);
        psState->bWaitingForI2c = false;
      } else { // still waiting for i2c bus to be ready
        return false;
      }
    }
  }

  // escape phase
  if (psState->u8SlaveAddr == 0x7f) {
    return true;
  }

  // send message phase (TX)
  if (lockmgr_acquire_lock(psIface->eBus, &psState->u32LastLabel)) {
    ++psState->u8SlaveAddr;
    i2c_write(psIface->eBus, psState->u8SlaveAddr, 0, NULL);
    psState->bWaitingForI2c = true;
  }
  return false;
}
