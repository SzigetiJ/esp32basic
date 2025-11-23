/*
 * Copyright 2024 - 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef SHT4X_H
#define SHT4X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "lockmgr.h"
#include "utils/i2ciface.h"

  // ============== Defines ==============
#define SHT40_CRC_POLY 0x31
#define SHT40_CRC_INIT 0xFF

  // ============== Types ==============

  /**
   * List of accepted SHT4X commands (documented in manual).
   */
  typedef enum {
    SHT4X_CMD_MEAS_H = 0,
    SHT4X_CMD_MEAS_M = 1,
    SHT4X_CMD_MEAS_L,
    SHT4X_CMD_SERIAL,
    SHT4X_CMD_RESET,
    SHT4X_CMD_HEAT200MW_1S,
    SHT4X_CMD_HEAT200MW_01S,
    SHT4X_CMD_HEAT110MW_1S,
    SHT4X_CMD_HEAT110MW_01S,
    SHT4X_CMD_HEAT20MW_1S,
    SHT4X_CMD_HEAT20MW_01S
  } ESht4xCommand;

  /**
   * Set of host -- device (ESP32 -- SHT40) communication states.
   */
  typedef enum {
    SHT4X_STATE_IDLE = 0,     ///< Idle state, nothing to be done.
    SHT4X_STATE_WR_SEND = 1,  ///< WRITE command to be sent (host to device).
    SHT4X_STATE_WR_RECV = 2,  ///< Waiting for write command to be finished (receive signal (not busy) from peripheral controller)
    SHT4X_STATE_RD_SEND = 3,  ///< READ command to be sent (host to device).
    SHT4X_STATE_RD_RECV = 4,  ///< Waiting for read command to be finished (receive signal (not busy) from peripheral controller)
    SHT4X_STATE_READY = 5,    ///< Communication done, result data arrived.
    SHT4X_STATE_ERROR = 6     ///< Something went wrong (probably I2C interrupt register shows error).
  } ESht4xCommState;

  /**
   * State descriptor of SHT4x connection.
   */
  typedef struct {
    ESht4xCommState eState;   ///< Current communication state
    ESht4xCommand eCommand;   ///< Current SHT4x command.
    uint32_t u32LockLabel;    ///< Label used in LockManager.
    uint8_t au8RxBuffer[6];   ///< Receive buffer.
    SI2cIfaceCfg sIface;      /// I2C communication interface.
  } SSht40StateDesc;

  // ============== Global values / References ==============

  // ============== Inline interface functions ==============

  static inline SSht40StateDesc sht40_init_descriptor(SI2cIfaceCfg sIface) {
    return (SSht40StateDesc){.eState = SHT4X_STATE_IDLE, .sIface = sIface};
  }

  /**
   * In idle state sets the SHT4X command that will be sent to the device (by calling sht4x_comm_cycle_ms()).
   * @param psState
   * @param eCommand
   * @return 
   */
  static inline bool sht40_set_command(SSht40StateDesc *psState, ESht4xCommand eCommand) {
    if (psState->eState != SHT4X_STATE_IDLE) return false;
    psState->eCommand = eCommand;
    psState->eState = SHT4X_STATE_WR_SEND;
    return true;
  }

  /**
   * Tells whether in current state I2C interaction (i.e., calling sht4x_comm_cycle_ms()) is needed
   * @param psState Current state
   * @return true: sht4x_comm_cycle_ms() should be called.
   */
  static inline bool sht40_needs_rxtx(const SSht40StateDesc *psState) {
    return SHT4X_STATE_WR_SEND <= psState->eState && psState->eState <= SHT4X_STATE_RD_RECV;
  }

  // ============== Interface functions ==============
  uint16_t sht4x_get_temp_raw(const SSht40StateDesc *psState);
  uint16_t sht4x_get_hum_raw(const SSht40StateDesc *psState);
  int32_t sht4x_get_temp(const SSht40StateDesc *psState);
  int32_t sht4x_get_hum(const SSht40StateDesc *psState);
  uint32_t sht4x_get_serial(const SSht40StateDesc *psState);
  bool sht4x_check_crc(const SSht40StateDesc *psState, bool bFirst);

  uint32_t sht4x_rxtx_cycle(SSht40StateDesc *psState);
#ifdef __cplusplus
}
#endif

#endif /* SHT4X_H */

