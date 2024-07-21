/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stdbool.h>
#include <string.h>
#include "typeaux.h"
#include "bh1750.h"


// #4: code constants
#define BH1750_CMD_POWERDOWN  0x00
#define BH1750_CMD_POWERON    0x01
#define BH1750_CMD_RESET      0x07
#define BH1750_CMD_CONT_MEASURE(X)    (0x10 | ((X) & 3))
#define BH1750_CMD_ONETIME_MEASURE(X) (0x20 | ((X) & 3))
#define BH1750_CMD_MODIFTMSB(X)       (0x40 | ((X) >> 5))
#define BH1750_CMD_MODIFTLSB(X)       (0x60 | ((X) & 0x1f))

#define BH1750_MTIME_REF      69U
#define BH1750_RES2MLX_MUL    (10000 / 12)
#define MEASTIME_H_REF_HMS    250U
#define MEASTIME_L_REF_HMS    36U

// ================= Internal Types ==================

typedef enum {
  STATE_POFF = 0,
  STATE_PON,
  STATE_ONETIME,
  STATE_CONTINUOUS
} EDeviceState;

typedef enum {
  DO_NOTHING = 0,
  DO_PDOWN,
  DO_PON,
  DO_RESET,
  DO_MODIF_MSB,
  DO_MODIF_LSB,
  DO_MEASURE,
  DO_READ
} EWhatToDo;

/**
 * Order of processing:
 * 1. PDOWN
 * 2. PON
 * 3. RESET
 * 4. MODIF_MSB
 * 5. MODIF_LSB
 * 6. MEASURE
 * 7. READ
 */
typedef struct {
  bool bReqPowerDown : 1;
  bool bReqPowerOn : 1;
  bool bReqReset : 1;
  bool bReqModifTMsb : 1;
  bool bReqModifTLsb : 1;
  bool bReqMeasurement : 1;
  bool bRead : 1;
  uint32_t rsvd7 : 1;

  EBh1750MeasRes e2MRes : 2;
  bool bContinuous : 1;
  uint32_t rsvd11 : 5;

  uint8_t u8MTime : 8;

  EDeviceState eDevState : 2;
  bool bWaitingForRx : 1;
  uint32_t rsvd27 : 5;
} BH1750Flags;

// ============ Internal data =================

// ============ Internal function declarations =================
static inline EWhatToDo _what_to_do(const BH1750Flags *psFlags);

// ============ Internal function definitions =================

static inline EWhatToDo _what_to_do(const BH1750Flags *psFlags) {
  return psFlags->bReqPowerDown ? DO_PDOWN :
          psFlags->bReqPowerOn ? DO_PON :
          psFlags->bReqReset ? DO_RESET :
          psFlags->bReqModifTMsb ? DO_MODIF_MSB :
          psFlags->bReqModifTLsb ? DO_MODIF_LSB :
          psFlags->bReqMeasurement ? DO_MEASURE :
          psFlags->bRead ? DO_READ :
          DO_NOTHING;
}

// ============ Interface function definitions =================

SBh1750StateDesc bh1750_init_state() {
  SBh1750StateDesc sRet;
  BH1750Flags *psFlags = (BH1750Flags*) & sRet.u32Flags;
  memset(&sRet, 0, sizeof (sRet));
  psFlags->u8MTime = BH1750_MTIME_REF;
  return sRet;

}

void bh1750_poweron(SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  psFlags->bReqPowerOn = true;
}

void bh1750_poweroff(SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  psFlags->bReqPowerDown = true;
}

void bh1750_reset(SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  if (psFlags->eDevState == STATE_POFF) {
    psFlags->bReqPowerOn = true;
  }
  psFlags->bReqReset = true;
}

void bh1750_measure(SBh1750StateDesc *psState, bool bContinuous, EBh1750MeasRes eMRes) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  psFlags->bReqMeasurement = true;
  psFlags->bContinuous = bContinuous;
  psFlags->e2MRes = eMRes;
}

void bh1750_read(SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  psFlags->bRead = true;
}

uint8_t bh1750_get_mtime(const SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  return psFlags->u8MTime;
}

EBh1750MeasRes bh1750_get_mres(const SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  return psFlags->e2MRes;
}

bool bh1750_is_continuous(const SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  return psFlags->bContinuous;
}

void bh1750_set_mtime(SBh1750StateDesc *psState, uint8_t u8Mtime) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  psFlags->u8MTime = u8Mtime;
  psFlags->bReqModifTMsb = true;
  psFlags->bReqModifTLsb = true;
}

uint32_t bh1750_result_to_mlx(uint16_t u16Result, uint8_t u8MTime, EBh1750MeasRes eRes) {
  return BH1750_RES2MLX_MUL * u16Result * BH1750_MTIME_REF / (u8MTime * (eRes == BH1750_RES_H2 ? 2 : 1));
}

uint32_t bh1750_measurementtime_hms(uint8_t u8MTime, EBh1750MeasRes eRes) {
  return (u8MTime * (eRes == BH1750_RES_L ? MEASTIME_L_REF_HMS : MEASTIME_H_REF_HMS)) / BH1750_MTIME_REF;
}

bool bh1750_is_poweroff(const SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  return psFlags->eDevState == STATE_POFF;
}

bool bh1750_is_poweron(const SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  return psFlags->eDevState == STATE_PON;
}

bool bh1750_async_rx_cycle(SBh1750StateDesc *psState, uint32_t *pu32hmsWaitHint) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);

  *pu32hmsWaitHint = 0U;
  if (psFlags->bWaitingForRx) {
    AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
    if (psEntry) {
      if (psEntry->bReady) {
        if (!(psEntry->u32IntSt & I2C_INT_MASK_ERR)) {
          EWhatToDo eTodo = _what_to_do(psFlags);

          switch (eTodo) {
            case DO_PDOWN:
              psFlags->bReqPowerDown = false;
              psFlags->eDevState = STATE_POFF;
              break;
            case DO_PON:
              psFlags->bReqPowerOn = false;
              psFlags->eDevState = STATE_PON;
              break;
            case DO_RESET:
              psFlags->bReqReset = false;
              break;
            case DO_MODIF_MSB:
              psFlags->bReqModifTMsb = false;
              break;
            case DO_MODIF_LSB:
              psFlags->bReqModifTLsb = false;
              break;
            case DO_MEASURE:
              psFlags->bReqMeasurement = false;
              psFlags->eDevState = psFlags->bContinuous ? STATE_CONTINUOUS : STATE_ONETIME;
              *pu32hmsWaitHint = bh1750_measurementtime_hms(psFlags->u8MTime, psFlags->e2MRes);
              break;
            case DO_READ:
              psFlags->bRead = false;
              if (psFlags->eDevState == STATE_ONETIME) {
                psFlags->eDevState = STATE_POFF;
              }
            default: // DO_NOTHING - error
              ; // TODO
          }
        } else {
          // no state change, retry TX
          // TODO: update retry counter
        }
        lockmgr_release_entry(psState->u32LastLabel);
        psFlags->bWaitingForRx = false;
      } else { // still waiting for i2c bus to be ready
        return false;
      }
    } else {
      // TODO: error, no lockmgr entry found
    }
  }
  return _what_to_do(psFlags) == DO_NOTHING;
}

bool bh1750_async_tx_cycle(const SI2cIfaceCfg *psIface, SBh1750StateDesc *psState) {
  BH1750Flags *psFlags = (BH1750Flags*) (&psState->u32Flags);
  EWhatToDo eTodo = _what_to_do(psFlags);

  if (psFlags->bWaitingForRx) return false;
  if (eTodo == DO_NOTHING) return false;
  if (!lockmgr_acquire_lock(psIface->eLck, &psState->u32LastLabel)) return false;

  AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
  bool bRet = true;

  if (eTodo != DO_READ) {
    uint8_t u8Data = eTodo == DO_PDOWN ? BH1750_CMD_POWERDOWN :
            eTodo == DO_PON ? BH1750_CMD_POWERON :
            eTodo == DO_RESET ? BH1750_CMD_RESET :
            eTodo == DO_MODIF_MSB ? BH1750_CMD_MODIFTMSB(psFlags->u8MTime) :
            eTodo == DO_MODIF_LSB ? BH1750_CMD_MODIFTLSB(psFlags->u8MTime) :
            eTodo == DO_MEASURE ? (psFlags->bContinuous ? BH1750_CMD_CONT_MEASURE(psFlags->e2MRes) : BH1750_CMD_ONETIME_MEASURE(psFlags->e2MRes)) :
            -1;
    i2c_write(psIface->eBus, psIface->u8SlaveAddr, 1, &u8Data);
  } else { // READ
    psEntry->pu8ReceiveBuffer = (uint8_t*) & psState->u16beResult;
    psEntry->u8RxLen = 2;
    i2c_read(psIface->eBus, psIface->u8SlaveAddr, 2);
  }
  if (bRet) { // kind of exception handling
    psFlags->bWaitingForRx = true;
  } else {
    lockmgr_release_entry(psState->u32LastLabel);
    lockmgr_free_lock(psIface->eLck);
  }
  return bRet;
}
