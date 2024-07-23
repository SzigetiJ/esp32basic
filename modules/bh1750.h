/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef BH1750_H
#define BH1750_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/i2ciface.h"

  /**
   * Possible measurement resolutions.
   */
  typedef enum {
    BH1750_RES_H = 0, ///< High resolution (unit: 1 lx) mode.
    BH1750_RES_H2 = 1, ///< High(er) resolution (unit: 0.5 lx) mode.
    BH1750_RES_L = 3 ///< Low resolution mode (unit 1 lx, steps: 4 units).
  } EBh1750MeasRes;

  /**
   * This function can be useful if you want to rotate through measurement resolutions.
   * @param eMRes Reference resolution value.
   * @return Next valid resolution value (loops back to the first item of the set after the last item).
   */
  static inline EBh1750MeasRes bh1750_measres_next(EBh1750MeasRes eMRes) {
    EBh1750MeasRes eRet = eMRes;
    ++eRet;
    if (eRet == 2) ++eRet;
    if (3 < eRet) eRet = 0;
    return eRet;
  }

  /**
   * State descriptor of the BH1750 device.
   */
  typedef struct {
    uint32_t u32LastLabel;
    uint32_t u32Flags;
    uint16_t u16beResult; ///< Measurement result in Big Endian format
  } SBh1750StateDesc;

  SBh1750StateDesc bh1750_init_state();

  // setters

  void bh1750_poweron(SBh1750StateDesc *psState);
  void bh1750_poweroff(SBh1750StateDesc *psState);
  void bh1750_reset(SBh1750StateDesc *psState);
  void bh1750_measure(SBh1750StateDesc *psState, bool bContinuous, EBh1750MeasRes eResolution);
  void bh1750_read(SBh1750StateDesc *psState);
  void bh1750_set_mtime(SBh1750StateDesc *psState, uint8_t u8Mtime);

  // getters

  uint8_t bh1750_get_mtime(const SBh1750StateDesc *psState);
  EBh1750MeasRes bh1750_get_mres(const SBh1750StateDesc *psState);
  bool bh1750_is_continuous(const SBh1750StateDesc *psState);

  bool bh1750_is_poweroff(const SBh1750StateDesc *psState);
  bool bh1750_is_poweron(const SBh1750StateDesc *psState);

  /**
   * Transforms measured illuminance value to mLx.
   * @param u16Result Raw measured value.
   * @param u8MTime Used measurement time value for the measurement.
   * @param eRes Measurement resolution.
   * @return Illuminance (in millilux).
   */
  uint32_t bh1750_result_to_mlx(uint16_t u16Result, uint8_t u8MTime, EBh1750MeasRes eRes);

  /**
   * Calculates measurement time (wait between sending Start measurement command and read out).
   * @param u8MTime Used measurement time value for the measurement.
   * @param eRes Measurement resolution.
   * @return Suggested wait time (somewhat mode than the typical value). Unit: 0.5ms.
   */
  uint32_t bh1750_measurementtime_hms(uint8_t u8MTime, EBh1750MeasRes eRes);

  /**
   * Receiver side of asynchronous communication.
   * @param psState Ptr. to BH1750 state descriptor.
   * @param pu32hmsWaitHint Ptr. to write the suggested wait time to (unit: 0.5ms).
   * @return No further actions needed (DO_NOTHING is next action).
   */
  bool bh1750_async_rx_cycle(SBh1750StateDesc *psState, uint32_t *pu32hmsWaitHint);
  bool bh1750_async_tx_cycle(const SI2cIfaceCfg *psIface, SBh1750StateDesc *psState);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */

