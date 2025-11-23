/*
 * Copyright 2024 - 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include "i2c.h"
#include "sht4x.h"
#include "utils/crc.h"

// ============== Defines ==============

// ============== Local types ==============

// ============== Local data ==============
static const uint8_t gau8Command[] = {
  0xFD, 0xF6, 0xE0,
  0x89, 0x94,
  0x39, 0x32,
  0x2F, 0x24,
  0x1E, 0x15
};

static const uint32_t gau32msWait[] = {
  9, 5, 2,
  1, 1,
  1100, 110,
  1100, 110,
  1100, 110
};
// ============== Internal function declarations ==============
static inline bool _command_has_data_response(ESht4xCommand eCommand);
static inline uint16_t _pu8_to_u16(const uint8_t *pu8Data);

// ============== Implementation ==============
// -------------- Internal functions --------------
/**
 * Tells whether a given command produces data to be read or not.
 * @param eCommand Given command.
 * @return The command produces data to be read.
 */
static inline bool _command_has_data_response(ESht4xCommand eCommand) {
  return eCommand != SHT4X_CMD_RESET;
}

/**
 * Converts the first two bytes of a (big endian) data stream to uint16_t.
 * @param pu8Data Input data stream with at least 2 bytes.
 * @return The first two bytes converted into uint16_t type.
 */
static inline uint16_t _pu8_to_u16(const uint8_t *pu8Data) {
  return (pu8Data[0] << 8) | (pu8Data[1] & 0xFF);
}

// -------------- Interface functions --------------
/**
 * Get raw temperature data.
 * Valid only in SHT4X_STATE_READY communication state
 * and for SHT4X_CMD_MEAS_* or SHT4X_CMD_HEAT* commands.
 * @param psState Current state.
 * @return Raw temperature data.
 */
uint16_t sht4x_get_temp_raw(const SSht40StateDesc *psState) {
  return _pu8_to_u16(psState->au8RxBuffer);
}

/**
 * Get raw humidity data.
 * Valid only in SHT4X_STATE_READY communication state
 * and for SHT4X_CMD_MEAS_* or SHT4X_CMD_HEAT* commands.
 * @param psState Current state.
 * @return Raw humidity data.
 */
uint16_t sht4x_get_hum_raw(const SSht40StateDesc *psState) {
  return _pu8_to_u16(psState->au8RxBuffer + 3);
}

/**
 * Get temperature value in 0.001°C resolution.
 * Valid only in SHT4X_STATE_READY communication state
 * and for SHT4X_CMD_MEAS_* or SHT4X_CMD_HEAT* commands.
 * @param psState Current state.
 * @return Temperature data in 0.001°C resolution.
 */
int32_t sht4x_get_temp(const SSht40StateDesc *psState) {
  return ((sht4x_get_temp_raw(psState) * 2734) >> 10) - 45000;
}

/**
 * Get relative humidity data in 0.001% resolution.
 * Valid only in SHT4X_STATE_READY communication state
 * and for SHT4X_CMD_MEAS_* or SHT4X_CMD_HEAT* commands.
 * @param psState Current state.
 * @return Humidity data in 0.001% resolution.
 */
int32_t sht4x_get_hum(const SSht40StateDesc *psState) {
  return ((sht4x_get_hum_raw(psState) * 1953) >> 10) - 6000;
}

/**
 * Get serial number.
 * Valid only in SHT4X_STATE_READY communication state
 * and for SHT4X_CMD_SERIAL command.
 * @param psState
 * @return Serial number.
 */
uint32_t sht4x_get_serial(const SSht40StateDesc *psState) {
  return (_pu8_to_u16(psState->au8RxBuffer) << 16) | _pu8_to_u16(psState->au8RxBuffer + 3);
}

/**
 * SHT4x devices send data with CRC checksum.
 * The 6 byte long responses are in format:
 * (1st data byte, 2nd data byte, checksum of the first data byte pair,
 * 3rd data byte, 4th data byte, checksum of the second data byte pair).
 * This function checks whether the a checksum is OK.
 * Valid only in SHT4X_STATE_READY communication state
 * and for any command except for SHT4X_CMD_RESET (reset does not produces any data response).
 * @param psState Current state.
 * @param bFirst True check the CRC of the first data byte pair, false: check the CRC of the second data pair.
 * @return CRC byte is OK.
 */
bool sht4x_check_crc(const SSht40StateDesc *psState, bool bFirst) {
  const uint8_t *pu8Data = psState->au8RxBuffer + (bFirst ? 0 : 3);
  uint8_t u8CrcRecv = psState->au8RxBuffer[bFirst ? 2 : 5];

  uint8_t u8CrcCalc = crc8(SHT40_CRC_POLY, SHT40_CRC_INIT, pu8Data, 2);
  return (u8CrcRecv == u8CrcCalc);
}

/**
 * This function is responsible for I2C communicating to SHT4x device.
 * It must be called multiple times once the a command is issued, until
 * SHT4X_STATE_READY or SHT4X_STATE_ERROR is reached.
 * It returns a hint saying when the next calling should be done (in ms).
 * @param psState Current state.
 * @return After how many ms the function should be called again.
 */
uint32_t sht4x_rxtx_cycle(SSht40StateDesc *psState) {
  uint32_t u32msWait = 0U;

  // recv side
  if (psState->eState == SHT4X_STATE_RD_RECV || psState->eState == SHT4X_STATE_WR_RECV) {
    AsyncResultEntry *psEntry = lockmgr_get_entry(psState->u32LockLabel);
    if (psEntry->bReady) {
      bool bErr = (0 < (psEntry->u32IntSt & I2C_INT_MASK_ERR));
      if (!bErr) {
        if (_command_has_data_response(psState->eCommand)) {
          ++psState->eState;
        } else {
          psState->eState = SHT4X_STATE_READY;
        }
      } else {
        // maybe repeat corresponding SEND
        psState->eState = SHT4X_STATE_ERROR;
      }
      lockmgr_release_entry(psState->u32LockLabel);
    } else {
      // I2C is still not ready.. maybe wait more time
      u32msWait = 1;
    }
  }

  // send side
  if (psState->eState == SHT4X_STATE_RD_SEND || psState->eState == SHT4X_STATE_WR_SEND) {
    if (lockmgr_acquire_lock(psState->sIface.eLck, &psState->u32LockLabel)) {
      switch (psState->eState) {
        case SHT4X_STATE_WR_SEND:
          i2c_write(psState->sIface.eBus, psState->sIface.u8SlaveAddr, 1, &gau8Command[psState->eCommand]);
          u32msWait = gau32msWait[psState->eCommand];
          break;
        case SHT4X_STATE_RD_SEND: // read data
          AsyncResultEntry *psEntry = lockmgr_get_entry(psState->u32LockLabel);
          psEntry->pu8ReceiveBuffer = psState->au8RxBuffer;
          psEntry->u8RxLen = 6;
          i2c_read(psState->sIface.eBus, psState->sIface.u8SlaveAddr, 6);
          break;
        default:
          // this branch should not be reached
      }
      ++psState->eState;
    }
  }

  return u32msWait;
}
