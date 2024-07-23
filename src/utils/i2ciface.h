#ifndef I2CIFACE_H
#define I2CIFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c.h"
#include "lockmgr.h"

  /**
   * Interface information how to access I2C slave device.
   */
  typedef struct {
    EI2CBus eBus;
    uint8_t u8SlaveAddr;
    ELockmgrResource eLck;
  } SI2cIfaceCfg;

#ifdef __cplusplus
}
#endif

#endif /* I2CIFACE_H */

