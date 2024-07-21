/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <string.h>
#include "i2c.h"
#include "bme280.h"
#include "typeaux.h"

#define MEMADDR_ID      0xd0  // unused
#define MEMADDR_RESET   0xe0
#define MEMADDR_CTRLH   0xf2
#define MEMADDR_STATUS  0xf3
#define MEMADDR_CTRLM   0xf4
#define MEMADDR_CONFIG  0xf5
#define MEMADDR_CALIB0  0x88
#define MEMADDR_CALIB1  0xe1
#define MEMADDR_DATA    0xF7

#define MEMLEN_CALIB0 26U
#define MEMLEN_CALIB1 16U
#define MEMLEN_DATA 8U

#define SYM_RESET 0xb6  ///< Reset symbol

// ================= Internal Types ==================

/**
 * Possible values of mode bits in ctrl_meas register
 */
typedef enum {
  MODE_SLEEP = 0U,
  MODE_FORCED = 1U,
  MODE_FORCED2 = 2U,
  MODE_NORMAL = 3U
} EMode;

/**
 * Registers at 0xf2..0xf5 store configuration in a structured way.
 * This type follows the structure of those registers.
 */
typedef struct {
  EBme280Osrs eOsrsH : 3;
  uint8_t rsvd3 : 5;
  bool bImUpdate : 1;
  uint8_t rsvd9 : 2;
  bool bMeasuring : 1;
  uint8_t rsvd12 : 4;
  EMode eMode : 2;
  EBme280Osrs eOsrsP : 3;
  EBme280Osrs eOsrsT : 3;
  bool bSpi3wEn : 1;
  bool rsvd25 : 1;
  EBme280Iir eFilter : 3;
  EBme280Tsb eTsb : 3;
} SConfigBytes;

/**
 * Calibration bytes in a simplified structure (i.e., some attribute types do not match (signed vs. unsigned)).
 */
typedef struct {
  int16_t digT[3];
  int16_t digP[9];
  uint8_t rsvd24;
  uint8_t digH1;
  int16_t digH2;
  uint8_t digH3; // calib[28]
  int8_t digH4Msb;
  uint8_t digH4Lsb : 4;
  int16_t digH5 : 12;
  int8_t digH6; // calib[32]
} SCalib;

/**
 * Handling of dirty/ready bits/bytes is as follows:
 *  0.) if bReset is active
 *   1.) write Reset, clear local config
 *  1.) if bModeSet is active
 *   1.) [write CtrlHum] - if dirty
 *   2.) [write CtrlConfig] - if dirty
 *   3.) write CtrlMeas
 *  2.) if bRequestForData is active
 *   1.) [read Calib0] - if not ready
 *   2.) [read Calib1] - if not ready
 *   3.) read Status - until updated
 *   4.) read Data
 *  */
typedef union {

  struct {
    bool bDirtyCtrlHum : 1;
    bool bDirtyStatus : 1; // meaning that the status bye must be read
    bool bDirtyCtrlMeas : 1; // note: async comm is not triggered until bModeSet is set.
    bool bDirtyConfig : 1;
    bool bDataUpdated : 1;
    bool bCalib0Ready : 1;
    bool bCalib1Ready : 1;
    bool bWaitingForRx : 1;
    bool bModeSet : 1; // triggers chain of state changes until the mode bits get written
    bool bRequestForData : 1; // triggers chain of state changes until data bytes are read out
    bool bReset : 1;  // triggers write to reset register
    uint32_t rsvd11 : 5;
    uint8_t u8CurAddr : 8;
    uint8_t u5CurLen : 5;
  };
  uint32_t raw;
} SSyncFlags;

// ============ Internal data =================
/**
 * EBme280Tsb => time mapping. Time unit is 0.5ms.
 */
uint32_t gau32hmsStandbyTime[] = {
  5 / 5,
  625 / 5,
  125000 / 5,
  250000 / 5,
  500000 / 5,
  1000000 / 5,
  10000 / 5,
  20000 / 5
};

/**
 * osrs value => number of oversampling mapping.
 */
uint8_t gau8Oversampling[] = {
  0,
  1U,
  2U,
  4U,
  8U,
  16U,
  16U,
  16U
};

// ============ Internal function declarations =================
static inline uint32_t _tmeasure_hms(const SConfigBytes *psConfig);
static SBme280TPH _transform_data(const uint8_t *pu8Data);
static inline int16_t _get_calib_h4(const SCalib *psCalib);
static int32_t _compensate_T(int32_t i32T, const SCalib *psCalib, uint32_t *t_fine);
static uint32_t _compensate_P(int32_t i32P, const SCalib *psCalib, uint32_t t_fine);
static uint32_t _compensate_H(int32_t i32H, const SCalib *psCalib, uint32_t t_fine);
static void _set_mode(SBme280StateDesc *psState, EMode eMode);
static inline void _write_byte(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState, uint8_t u8MemAddr, uint8_t u8Value);
static void _write_cfgbyte(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState, uint8_t u8MemAddr);
static void _read_bytes(const SI2cIfaceCfg *psIface, AsyncResultEntry* psEntry, SBme280StateDesc *psState, uint8_t *pu8Dest, uint8_t u8MemAddr, uint8_t u8MemLen);

// ============ Internal function definitions =================
/**
 * Estimate the typical measurement time (t_{measure,typ}) based on oversampling settings.
 * @param psConfig Stores the oversampling settings. Other attributes are not used.
 * @return Estimated measurement time (unit: 0.5 ms).
 */
static inline uint32_t _tmeasure_hms(const SConfigBytes *psConfig) {
  return 2 +
  4 * gau8Oversampling[psConfig->eOsrsT] +
          (psConfig->eOsrsP ? (4 * gau8Oversampling[psConfig->eOsrsP] + 1) : 0) +
          (psConfig->eOsrsH ? (4 * gau8Oversampling[psConfig->eOsrsH] + 1) : 0);
}

static SBme280TPH _transform_data(const uint8_t *pu8Data) {
  SBme280TPH sRet = {
    ((pu8Data[3] << 12) | (pu8Data[4] << 4) | (pu8Data[5] >> 4)),
    ((pu8Data[0] << 12) | (pu8Data[1] << 4) | (pu8Data[2] >> 4)),
    (pu8Data[6] << 8) | (pu8Data[7])
  };
  return sRet;
}

/**
 * Calibration attribute dig_H4 is composed of low (4 bits) and high (8 bits) parts.
 * This inline function calculates its value.
 * @param psCalib Full calibration data.
 * @return dig_H4 attribute.
 */
static inline int16_t _get_calib_h4(const SCalib *psCalib) {
  return psCalib->digH4Msb << 4 | psCalib->digH4Lsb;
}

static int32_t _compensate_T(int32_t i32T, const SCalib *psCalib, uint32_t *pu32TFine) {
  int32_t var1, var2, T;
  int32_t i32ax = (i32T >> 3) - (psCalib->digT[0] << 1);
  var1 = i32ax * psCalib->digT[1] >> 11;
  var2 = (((i32ax / 2) * (i32ax / 2)) >> 12) * psCalib->digT[2] >> 14;
  *pu32TFine = var1 + var2;
  T = (*pu32TFine * 5 + 128) >> 8;
  return T;
}

static uint32_t _compensate_P(int32_t i32P, const SCalib *psCalib, uint32_t u32TFine) {
  int64_t i64DT = (int64_t) u32TFine - 128000; // diff from 25°C
  int64_t i64DP = 1048576 - i32P;

  int64_t i64DTPolyA = (i64DT * i64DT * (int64_t) psCalib->digP[2] >> 8)
          + (i64DT * (int64_t) psCalib->digP[1] << 12)
          + ((int64_t) 1 << 47);
  i64DTPolyA *= (uint16_t) psCalib->digP[0];
  i64DTPolyA >>= 33;
  int64_t i64DTPolyB = (i64DT * i64DT * (int64_t) psCalib->digP[5])
          + (i64DT * (int64_t) psCalib->digP[4] << 17)
          + ((int64_t) psCalib->digP[3] << 35);
  if (i64DTPolyA == 0) {
    return 0; // avoid exception caused by division by zero
  }
  int64_t i64Px = (((i64DP << 31) - i64DTPolyB) * 3125) / i64DTPolyA;
  int64_t i64PxPoly = ((int64_t) psCalib->digP[8] * (i64Px >> 13) * (i64Px >> 13) >> 25)
          + ((int64_t) psCalib->digP[7] * i64Px >> 19)
          + i64Px;
  i64PxPoly >>= 8;
  i64PxPoly += (int64_t) psCalib->digP[6] << 4;
  return (int32_t) i64PxPoly;
}

static uint32_t _compensate_H(int32_t i32H, const SCalib *psCalib, uint32_t u32TFine) {
  int32_t v_x1_u32r;
  v_x1_u32r = u32TFine - (int32_t) 76800; // diff from 15°C
  v_x1_u32r = (((((i32H << 14) - ((int32_t) _get_calib_h4(psCalib) << 20)
          - (int32_t) psCalib->digH5 * v_x1_u32r) + (int32_t) 16384) >> 15) * (((((((v_x1_u32r *
          (int32_t) psCalib->digH6) >> 10) * (((v_x1_u32r * (int32_t) psCalib->digH3) >> 11) +
          (int32_t) 32768)) >> 10) + (int32_t) 2097152) * (int32_t) psCalib->digH2 +
          8192) >> 14));
  v_x1_u32r = (v_x1_u32r - ((((v_x1_u32r >> 15) * (v_x1_u32r >> 15) >> 7) * (int32_t) psCalib->digH1) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  return (uint32_t) (v_x1_u32r >> 12);
}

static SBme280TPH _compensate(SBme280TPH sRaw, const SCalib *psCalib, uint32_t *pu32TFine) {
  int32_t i32T = _compensate_T(sRaw.i32Temp, psCalib, pu32TFine); // evaluation of pu32TFine must preceed using of that value in P/H compensation
  return (SBme280TPH){
    i32T,
    _compensate_P(sRaw.i32Pres, psCalib, *pu32TFine),
    _compensate_H(sRaw.i32Hum, psCalib, *pu32TFine)};
}

static void _set_mode(SBme280StateDesc *psState, EMode eMode) {
  ((SConfigBytes*) psState->au8Config)->eMode = eMode;
  ((SSyncFlags*) & psState->u32CommState)->bDirtyCtrlMeas = true;
  ((SSyncFlags*) & psState->u32CommState)->bModeSet = true;
}

/**
 * Writes a single byte into a given register on the given slave I2C device,
 * and updates the internal state.
 * @param psIface Interface information.
 * @param psState Internal state (to update).
 * @param u8MemAddr Register address.
 * @param u8Value Value to write into the register.
 */
static inline void _write_byte(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState, uint8_t u8MemAddr, uint8_t u8Value) {
  uint8_t au8Buf[] = {
    u8MemAddr,
    u8Value
  };
  i2c_write(psIface->eBus, psIface->u8SlaveAddr, 2, au8Buf);
  ((SSyncFlags*) & psState->u32CommState)->u8CurAddr = u8MemAddr;
  ((SSyncFlags*) & psState->u32CommState)->u5CurLen = 1U;
}

/**
 * Writes local configuration data byte to the given register on the device.
 * Wrapper to _write_byte().
 * @param psIface Interface information.
 * @param psState Internal state (local configuration data is stored here).
 * @param u8MemAddr Register address.
 */
static void _write_cfgbyte(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState, uint8_t u8MemAddr) {
  _write_byte(psIface, psState, u8MemAddr, psState->au8Config[u8MemAddr - MEMADDR_CTRLH]);
}

/**
 * Triggers reading a sequence of data from the device registers.
 * @param psIface Interface information.
 * @param psEntry Asynchronous communication entity (stores where to write to bytes read from the slave device).
 * @param psState Internal state (is updated).
 * @param pu8Dest Memory address where to write read data.
 * @param u8MemAddr Register address.
 * @param u8MemLen Read length.
 */
static void _read_bytes(const SI2cIfaceCfg *psIface, AsyncResultEntry* psEntry, SBme280StateDesc *psState, uint8_t *pu8Dest, uint8_t u8MemAddr, uint8_t u8MemLen) {
  psEntry->pu8ReceiveBuffer = pu8Dest;
  psEntry->u8RxLen = u8MemLen;
  i2c_read_mem(psIface->eBus, psIface->u8SlaveAddr, u8MemAddr, u8MemLen);
  ((SSyncFlags*) & psState->u32CommState)->u8CurAddr = u8MemAddr;
  ((SSyncFlags*) & psState->u32CommState)->u5CurLen = u8MemLen;
}

// ============ Interface function definitions =================

SBme280StateDesc bme280_init_state() {
  SBme280StateDesc sRet;
  memset(&sRet, 0, sizeof (sRet));
  return sRet;
};

bool bme280_set_osrs_h(SBme280StateDesc *psState, EBme280Osrs eOsrsH) {
  if (bme280_get_osrs_h(psState) == eOsrsH) return false;
  ((SConfigBytes*) psState->au8Config)->eOsrsH = eOsrsH;
  ((SSyncFlags*) & psState->u32CommState)->bDirtyCtrlHum = true;
  return true;
};

bool bme280_set_osrs_t(SBme280StateDesc *psState, EBme280Osrs eOsrsT) {
  if (bme280_get_osrs_t(psState) == eOsrsT) return false;
  ((SConfigBytes*) psState->au8Config)->eOsrsT = eOsrsT;
  ((SSyncFlags*) & psState->u32CommState)->bDirtyCtrlMeas = true;
  return true;
}

bool bme280_set_osrs_p(SBme280StateDesc *psState, EBme280Osrs eOsrsP) {
  if (bme280_get_osrs_p(psState) == eOsrsP) return false;
  ((SConfigBytes*) psState->au8Config)->eOsrsP = eOsrsP;
  ((SSyncFlags*) & psState->u32CommState)->bDirtyCtrlMeas = true;
  return true;
}

EBme280Osrs bme280_get_osrs_h(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->eOsrsH;
}

EBme280Osrs bme280_get_osrs_t(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->eOsrsT;
}

EBme280Osrs bme280_get_osrs_p(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->eOsrsP;
}

bool bme280_set_config(SBme280StateDesc *psState, EBme280Tsb eTsb, EBme280Iir eFilter, bool bSpi3wEn) {
  ((SConfigBytes*) psState->au8Config)->eTsb = eTsb;
  ((SConfigBytes*) psState->au8Config)->eFilter = eFilter;
  ((SConfigBytes*) psState->au8Config)->bSpi3wEn = bSpi3wEn;
  ((SSyncFlags*) & psState->u32CommState)->bDirtyConfig = true;
  return true;
}

EBme280Tsb bme280_get_tsb(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->eTsb;
}

EBme280Iir bme280_get_filter(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->eFilter;
}

bool bme280_get_spi3wen(const SBme280StateDesc *psState) {
  return ((const SConfigBytes*) psState->au8Config)->bSpi3wEn;
}

bool bme280_is_data_updated(const SBme280StateDesc *psState) {
  return ((const SSyncFlags*) &psState->u32CommState)->bDataUpdated;
}

void bme280_ack_data_updated(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bDataUpdated = false;
}

SBme280TPH bme280_get_measurement(const SBme280StateDesc *psState, uint32_t *pu32TFine) {
  return bme280_calc_measeurement(psState->au8Data, psState->au8Calib, pu32TFine);
}

SBme280TPH bme280_calc_measeurement(const uint8_t *pu8Data, const uint8_t *pu8Calib, uint32_t *pu32TFine) {
  uint32_t u32TFine;
  SBme280TPH sRaw = _transform_data(pu8Data);
  SBme280TPH sRet = _compensate(sRaw, (const SCalib*) pu8Calib, &u32TFine);

  if (pu32TFine != NULL) {
    *pu32TFine = u32TFine;
  }
  return sRet;
}

void bme280_set_mode_forced(SBme280StateDesc *psState) {
  _set_mode(psState, MODE_FORCED);
}

void bme280_set_mode_normal(SBme280StateDesc *psState) {
  _set_mode(psState, MODE_NORMAL);
}

void bme280_set_mode_sleep(SBme280StateDesc *psState) {
  _set_mode(psState, MODE_SLEEP);
}

void bme280_reset(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bReset = true;
}

bool bme280_is_resetting(const SBme280StateDesc *psState) {
  return ((const SSyncFlags*) & psState->u32CommState)->bReset;
}

bool bme280_async_rx_cycle(SBme280StateDesc *psState, uint32_t *pu32hmsWaitHint) {
  SSyncFlags *psFlags = (SSyncFlags*) & psState->u32CommState;
  SConfigBytes *psConf = (SConfigBytes*) psState->au8Config;
  bool bRet = false;

  *pu32hmsWaitHint = 0U;
  if (psFlags->bWaitingForRx) {
    AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
    if (psEntry) {
      if (psEntry->bReady) {
        if (!(psEntry->u32IntSt & I2C_INT_MASK_ERR)) {
          switch (psFlags->u8CurAddr) {
            case MEMADDR_RESET: // write
              psFlags->bReset = false;
              *(uint32_t*) psState->au8Config = 0;
              break;
            case MEMADDR_CTRLH: // write
              psFlags->bDirtyCtrlHum = false;
              break;
            case MEMADDR_CONFIG: // write
              psFlags->bDirtyConfig = false;
              break;
            case MEMADDR_CTRLM: // write
              psFlags->bDirtyCtrlMeas = false;
              psFlags->bModeSet = false;
              if (psConf->eMode != MODE_SLEEP) {
                psFlags->bDirtyStatus = true;
                psFlags->bRequestForData = true;
                *pu32hmsWaitHint = _tmeasure_hms(psConf);
              }
              break;
            case MEMADDR_STATUS: // read, requires check
              if (!psConf->bMeasuring) {
                psFlags->bDirtyStatus = false;
              }
              break;
            case MEMADDR_CALIB0: // read
              psFlags->bCalib0Ready = true;
              break;
            case MEMADDR_CALIB1: // read
              psFlags->bCalib1Ready = true;
              break;
            case MEMADDR_DATA: // read
              psFlags->bDataUpdated = true;
              if (psConf->eMode != MODE_NORMAL) {
                psFlags->bRequestForData = false;
              } else {
                *pu32hmsWaitHint = gau32hmsStandbyTime[psConf->eTsb];
              }
              break;
            default: // unexpected address
              ; // TODO
          }
          bRet = true;
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
  return bRet;
}

bool bme280_async_tx_cycle(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState) {
  SSyncFlags *psFlags = (SSyncFlags*) & psState->u32CommState;

  if (psFlags->bWaitingForRx) return false;

  bool bWrite = psFlags->bModeSet || psFlags->bReset;
  bool bRead = psFlags->bRequestForData;

  if (!bWrite && !bRead) return false;
  if (!lockmgr_acquire_lock(psIface->eLck, &psState->u32LastLabel)) return false;
  AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
  bool bRet = true;

  if (bWrite) {
    if (psFlags->bReset) {
      _write_byte(psIface, psState, MEMADDR_RESET, SYM_RESET);
    } else if (psFlags->bDirtyCtrlHum) {
      _write_cfgbyte(psIface, psState, MEMADDR_CTRLH);
    } else if (psFlags->bDirtyConfig) {
      _write_cfgbyte(psIface, psState, MEMADDR_CONFIG);
    } else if (psFlags->bDirtyCtrlMeas) {
      _write_cfgbyte(psIface, psState, MEMADDR_CTRLM);
    } else {
      // what to write?
      bRet = false;
    }
  } else if (bRead) {
    if (!psFlags->bCalib0Ready) {
      _read_bytes(psIface, psEntry, psState, psState->au8Calib, MEMADDR_CALIB0, MEMLEN_CALIB0);
    } else if (!psFlags->bCalib1Ready) {
      _read_bytes(psIface, psEntry, psState, psState->au8Calib + MEMLEN_CALIB0, MEMADDR_CALIB1, MEMLEN_CALIB1);
    } else if (psFlags->bDirtyStatus) {
      _read_bytes(psIface, psEntry, psState, psState->au8Config + MEMADDR_STATUS - MEMADDR_CTRLH, MEMADDR_STATUS, 1);
    } else if (!psFlags->bDataUpdated) {
      _read_bytes(psIface, psEntry, psState, psState->au8Data, MEMADDR_DATA, MEMLEN_DATA);
    } else {
      // what to read?
      bRet = false;
    }
  } else {
    // unexpected branch
    bRet = false;
  }
  if (bRet) { // kind of exception handling
    psFlags->bWaitingForRx = true;
  } else {
    lockmgr_release_entry(psState->u32LastLabel);
    lockmgr_free_lock(psIface->eLck);
  }
  return bRet;
}
