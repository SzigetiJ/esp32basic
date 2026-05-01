/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <stddef.h>
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
  EBme280Mode eMode : 2;
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

typedef enum {
  COMM_RD_CALIB0 = 0,
  COMM_RD_ID,
  COMM_RD_CALIB1,
  COMM_RD_CFG,
  COMM_RD_STATUS,
  COMM_RD_DATA,
  COMM_WR_RESET,
  COMM_WR_CFG
} ECommAddr;

/**
 * Handling of set/get bits is as follows:
 *  0.) if bSetReset is active
 *   1.) write Reset, then clear local memory mirror
 *  1.) if bSetCtrlHum or bSetCtrlMeas or bSetConfig is active
 *   1.) write the marked registers with data from ConfigOut, then copy the marked registers form ConfigOut to ConfigMirror
 *  2.) if and bGetXxx flag is active, read the corresponding registers into the XxxMirror are in the following order:
 *   1.) [read Calib0] - if bGetCalib0 is set,
 *   2.) [read Id] - if bGetId is set,
 *   3.) [read Calib1] - if bGetCalib1 is set,
 *   4.) [read Data] - if bGetData is set,
 *   5.) [read Config] - if any of bGetCtrlHum, bGetCtrlMeas or bGetConfig is set, but bGetStatus is not set,
 *   6.) [read Status] - if bGetStatus is set.
 *  */
typedef union {

  struct {
    ///---1st byte
    // WR side flags
    bool bSetCtrlHum : 1;   ///< ctrl_hum register should be written to peripheral
    bool bSetReset : 1;     ///< reset register should be written to peripheral
    bool bSetCtrlMeas : 1;  ///< ctrl_mes register should be written to peripheral
    bool bSetConfig : 1;    ///< config register should be written to peripheral
    uint32_t rsvd4 : 4;
    //---2nd byte
    // RD side flags
    bool bGetCalib0 : 1;    ///< calib00..calib25 registers should be read from peripheral
    bool bGetId : 1;        ///< id register should be read from peripheral
    bool bGetCalib1 : 1;    ///< callib26..calib41 registers should be read from peripheral
    bool bGetConfig : 1;    ///< config register should be read from peripheral
    bool bGetStatus : 1;    ///< status register should be read from peripheral
    bool bGetData : 1;      ///< measurement data registers should be read from peripheral
    uint32_t rsvd14 : 2;
    //---3rd byte
    bool bWaitingForRx : 1; ///< I2C R/W communication was triggered by async TX, and the RX has not / could not mark it as done.
    uint32_t rsvd17 : 7;
    //---4th byte
    ECommAddr u4UpdAddr : 4;  ///< What R/W communication is going on / vaild if bWaitingForRx is true.
    uint8_t u4UpdFlag : 4;    ///< The RX process requires parameters/flags for some R/W operation (e.g., ctrl/conf reg write).
  } ;

  struct {
    uint8_t u8Setters;
    uint8_t u8Getters;
    uint8_t u8CommFlags;
    uint8_t u8Params;
  } ;
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

const uint8_t gau8ReadAddr[] = {
  MEMADDR_CALIB0,
  MEMADDR_ID,
  MEMADDR_CALIB1,
  MEMADDR_CTRLH,
  MEMADDR_STATUS,
  MEMADDR_DATA
};

const uint8_t gau8ReadLen[] = {
  MEMLEN_CALIB0,
  1,
  MEMLEN_CALIB1,
  4,
  1,
  MEMLEN_DATA
};

const uint8_t gau8MirrorOffset[] = {
  offsetof(SBme280StateDesc, au8CalibMirror),
  offsetof(SBme280StateDesc, u8IdMirror),
  offsetof(SBme280StateDesc, au8CalibMirror) + MEMLEN_CALIB0,
  offsetof(SBme280StateDesc, au8ConfigMirror),
  offsetof(SBme280StateDesc, au8ConfigMirror) + 1,
  offsetof(SBme280StateDesc, au8DataMirror)
};

/**
 * maps EBme280MetricSelector i to the config register index of osrs_i.
 */
const uint8_t gau8OsrsCfgReg[] = {
  0, 2, 2
};

/**
 * maps EBme280MetricSelector i to bit shift of osrs_i within the corresponding config register.
 */
const uint8_t gau8OsrsRegShift[] = {
  0, 2, 5
};


// ============ Internal function declarations =================
static inline uint32_t _tmeasure_hms(const SConfigBytes *psConfig);
static SBme280TPH _transform_data(const uint8_t *pu8Data);
static inline int16_t _get_calib_h4(const SCalib *psCalib);
static int32_t _compensate_T(int32_t i32T, const SCalib *psCalib, uint32_t *t_fine);
static uint32_t _compensate_P(int32_t i32P, const SCalib *psCalib, uint32_t t_fine);
static uint32_t _compensate_H(int32_t i32H, const SCalib *psCalib, uint32_t t_fine);
static void _write_reset(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState);
static void _write_cfg(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState);
static void _read_bytes(const SI2cIfaceCfg *psIface, AsyncResultEntry* psEntry, SBme280StateDesc *psState, uint8_t *pu8Dest, ECommAddr eCommAdd);

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

/**
 * Makes BME280 I2C write to reset register with reset symbol.
 * @param psIface I2C interface.
 * @param psState State descriptor.
 */
static void _write_reset(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState) {
  uint8_t au8Buf[] = {
    MEMADDR_RESET,
    SYM_RESET
  };
  i2c_write(psIface->eBus, psIface->u8SlaveAddr, 2, au8Buf);
  ((SSyncFlags*) & psState->u32CommState)->u4UpdAddr = COMM_WR_RESET;
  ((SSyncFlags*) & psState->u32CommState)->u4UpdFlag = 0U;
}

/**
 * Makes BME280 I2C writes to the config registers (ctrl_hum, ctrl_meas and config)
 * with data taken from psState->au8ConfigOut. Note, if any setter
 * (i.e., bSetCtrlHum, bSetCtrlMeas or bSetConfig) is unset, the
 * corresponding register will not be written (it will be skipped).
 * @param psIface I2C interface.
 * @param psState State descriptor.
 */
static void _write_cfg(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState) {
  uint8_t au8Buf[6];
  uint8_t u8BufIdx = 0;
  uint8_t u8UpdFlag = ((SSyncFlags*)(&psState->u32CommState))->u8Setters & 0x0d;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) continue; // skipping checking of bSetReset and writing to status register
    if (u8UpdFlag & (1 << i)) {
      au8Buf[u8BufIdx] = MEMADDR_CTRLH + i;
      au8Buf[u8BufIdx + 1] = psState->au8ConfigOut[i];
      u8BufIdx += 2;
    }
  }
  i2c_write(psIface->eBus, psIface->u8SlaveAddr, u8BufIdx, au8Buf);
  ((SSyncFlags*) & psState->u32CommState)->u4UpdAddr = COMM_WR_CFG;
  ((SSyncFlags*) & psState->u32CommState)->u4UpdFlag = u8UpdFlag;
}

/**
 * Triggers reading a sequence of data from the device registers.
 * @param psIface Interface information.
 * @param psEntry Asynchronous communication entity (stores where to write to bytes read from the slave device).
 * @param psState Internal state (is updated).
 * @param pu8Dest Memory address where to write read data.
 * @param eCommAddr 'what-to-read' selector. Implicitly defines the read register area.
 */
static void _read_bytes(const SI2cIfaceCfg *psIface, AsyncResultEntry* psEntry, SBme280StateDesc *psState, uint8_t *pu8Dest, ECommAddr eCommAddr) {
  uint8_t u8MemAddr = gau8ReadAddr[eCommAddr];
  uint8_t u8MemLen = gau8ReadLen[eCommAddr];

  psEntry->pu8ReceiveBuffer = pu8Dest;
  psEntry->u8RxLen = u8MemLen;
  i2c_read_mem(psIface->eBus, psIface->u8SlaveAddr, u8MemAddr, u8MemLen);
  ((SSyncFlags*) & psState->u32CommState)->u4UpdAddr = eCommAddr;
  ((SSyncFlags*) & psState->u32CommState)->u4UpdFlag = 0;
}

/**
 * Total memory cleanup of the state descriptor,
 * setting the mirrored register space to initial state.
 * @param psState State descriptor to cleanup.
 */
void _clear_state(SBme280StateDesc *psState) {
  memset(psState, 0, sizeof (SBme280StateDesc));
  psState->au8DataMirror[0] = 0x80;
  psState->au8DataMirror[3] = 0x80;
  psState->au8DataMirror[6] = 0x80;
}
// ============ Interface function definitions =================

SBme280StateDesc bme280_init_state() {
  SBme280StateDesc sRet;
  _clear_state(&sRet);
  return sRet;
};

/**
 * Sets an osrs parameter in local config register and schedules write to the peripheral.
 * @param psState State descriptor.
 * @param eMetric Measurement metric selector.
 * @param eOsrs Value to set.
 */
void bme280_set_osrs(SBme280StateDesc *psState, EBme280MetricSelector eMetric, EBme280Osrs eOsrs) {
  psState->au8ConfigOut[gau8OsrsCfgReg[eMetric]] &= ~(7 << gau8OsrsRegShift[eMetric]);
  psState->au8ConfigOut[gau8OsrsCfgReg[eMetric]] |= eOsrs << gau8OsrsRegShift[eMetric];
  psState->u32CommState |= 1 << gau8OsrsCfgReg[eMetric];
}

/**
 * Sets measurement mode in local config register and schedules write to the peripheral.
 * @param psState State descriptor.
 * @param eMode Value to set.
 */
void bme280_set_mode(SBme280StateDesc *psState, EBme280Mode eMode) {
  ((SConfigBytes*) psState->au8ConfigOut)->eMode = eMode;
  ((SSyncFlags*) & psState->u32CommState)->bSetCtrlMeas = true;
}

/**
 * Retrieves osrs value from local register.
 * @param psState State descriptor.
 * @param bMirror Selects mirror (received from peripheral) (true) or outgoing (to be written to peripheral) (false) local register.
 * @param eMetric Selects the measurement metric.
 * @return Current osrs value of the given metric.
 */
EBme280Osrs bme280_get_osrs(const SBme280StateDesc *psState, bool bMirror, EBme280MetricSelector eMetric) {
  const uint8_t *pu8Regs = bMirror ? psState->au8ConfigMirror : psState->au8ConfigOut;
  return (pu8Regs[gau8OsrsCfgReg[eMetric]] >> gau8OsrsRegShift[eMetric]) & 7;
}

/**
 * Retrieves mode value from local register.
 * @param psState State descriptor.
 * @param bMirror Selects mirror (received from peripheral) (true) or outgoing (to be written to peripheral) (false) local register.
 * @return Current mode value.
 */
EBme280Mode bme280_get_mode(const SBme280StateDesc *psState, bool bMirror) {
  const uint8_t *pu8Regs = bMirror ? psState->au8ConfigMirror : psState->au8ConfigOut;
  return pu8Regs[2] & 3;
}

bool bme280_set_config(SBme280StateDesc *psState, EBme280Tsb eTsb, EBme280Iir eFilter, bool bSpi3wEn) {
  ((SConfigBytes*) psState->au8ConfigOut)->eTsb = eTsb;
  ((SConfigBytes*) psState->au8ConfigOut)->eFilter = eFilter;
  ((SConfigBytes*) psState->au8ConfigOut)->bSpi3wEn = bSpi3wEn;
  ((SSyncFlags*) & psState->u32CommState)->bSetConfig = true;
  return true;
}

EBme280Tsb bme280_get_tsb(const SBme280StateDesc *psState, bool bMirror) {
  const uint8_t *pu8Regs = bMirror ? psState->au8ConfigMirror : psState->au8ConfigOut;
  return ((const SConfigBytes*) pu8Regs)->eTsb;
}

EBme280Iir bme280_get_filter(const SBme280StateDesc *psState, bool bMirror) {
  const uint8_t *pu8Regs = bMirror ? psState->au8ConfigMirror : psState->au8ConfigOut;
  return ((const SConfigBytes*) pu8Regs)->eFilter;
}

bool bme280_get_spi3wen(const SBme280StateDesc *psState, bool bMirror) {
  const uint8_t *pu8Regs = bMirror ? psState->au8ConfigMirror : psState->au8ConfigOut;
  return ((const SConfigBytes*) pu8Regs)->bSpi3wEn;
}

/**
 * Schedules reset (write operation).
 * @param psState State descriptor.
 */
void bme280_reset(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bSetReset = true;
}

/**
 * Schedules read of id register.
 * @param psState State descriptor.
 */
void bme280_req_id(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bGetId = true;
}

/**
 * Schedules read of status register.
 * @param psState State descriptor.
 */
void bme280_req_status(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bGetStatus = true;
}

/**
 * Schedules read of the three config and the status registers.
 * @param psState State descriptor.
 */
void bme280_req_config(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bGetConfig = true;
}

/**
 * Schedules read of calib00..41 registers.
 * @param psState State descriptor.
 */
void bme280_req_calib(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bGetCalib0 = true;
  ((SSyncFlags*) & psState->u32CommState)->bGetCalib1 = true;
}

/**
 * Schedules read of data registers.
 * @param psState State descriptor.
 */
void bme280_req_data(SBme280StateDesc *psState) {
  ((SSyncFlags*) & psState->u32CommState)->bGetData = true;
}

/**
 * Tells whether reset is scheduled or not.
 * @param psState State descriptor.
 * @return Reset is scheduled.
 */
bool bme280_is_resetting(const SBme280StateDesc *psState) {
  return ((const SSyncFlags*) & psState->u32CommState)->bSetReset;
}

/**
 * Gets id from mirrored register.
 * Note, first, data must be read from peripheral, see bme280_req_id()!
 * @param psState State descriptor.
 * @return id - received from peripheral.
 */
uint8_t bme280_get_id(const SBme280StateDesc *psState) {
  return psState->u8IdMirror;
}

uint8_t bme280_get_status(const SBme280StateDesc *psState) {
  return psState->au8ConfigMirror[1] & 0x09;
}

bool bme280_is_data_ready(const SBme280StateDesc *psState) {
  return !((const SSyncFlags*) &psState->u32CommState)->bGetData;
}

SBme280TPH bme280_get_measurement(const SBme280StateDesc *psState, uint32_t *pu32TFine) {
  return bme280_calc_measurement(psState->au8DataMirror, psState->au8CalibMirror, pu32TFine);
}

SBme280TPH bme280_calc_measurement(const uint8_t *pu8Data, const uint8_t *pu8Calib, uint32_t *pu32TFine) {
  uint32_t u32TFine;
  SBme280TPH sRaw = _transform_data(pu8Data);
  SBme280TPH sRet = _compensate(sRaw, (const SCalib*) pu8Calib, &u32TFine);

  if (pu32TFine != NULL) {
    *pu32TFine = u32TFine;
  }
  return sRet;
}

bool bme280_has_async_todo(const SBme280StateDesc *psState) {
  return psState->u32CommState & 0xFFFF;
}

bool bme280_is_waiting(const SBme280StateDesc *psState) {
  return ((const SSyncFlags*) & psState->u32CommState)->bWaitingForRx;
}

uint32_t bme280_measurement_duration_hms(const SBme280StateDesc *psState) {
  return _tmeasure_hms((const SConfigBytes*)psState->au8ConfigMirror);
}

/**
 * Check Peripheral state.
 * @param psState
 * @param pu32hmsWaitHint
 * @return 
 */
bool bme280_async_rx_cycle(SBme280StateDesc *psState, uint32_t *pu32hmsWaitHint) {
  SSyncFlags *psFlags = (SSyncFlags*) & psState->u32CommState;
  bool bRet = false;

  *pu32hmsWaitHint = 0U;

  // check I.)
  if (!psFlags->bWaitingForRx) {
    return false;
  }

  // check II.)
  AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
  if (!psEntry) {
    // TODO: this is a serious error: no lockmgr entry found
    return false;
  }

  // check III.)
  if (!psEntry->bReady) { // still waiting for i2c bus to be ready
    return false;
  }

  // At this point it is sure, that the communication is over (ready).
  // check IV.)
  if (psEntry->u32IntSt & I2C_INT_MASK_ERR) { // failure
    // TODO: store error code
  } else {  // success

    // some local data must be update,
    // e.g., unset todo flags,
    // migrate data to mirror memory area
    switch (psFlags->u4UpdAddr) {
        // setters
      case COMM_WR_RESET:
        psFlags->bSetReset = false;
        // store local values before cleanup
        uint32_t u32TmpCommState = psState->u32CommState;
        uint32_t u32TmpLastLabel = psState->u32LastLabel;
        uint32_t u32TmpConfigOut = *((uint32_t*)psState->au8ConfigOut);
        // cleanup
        _clear_state(psState);
        // restore local values after cleanup
        psState->u32CommState = u32TmpCommState;
        psState->u32LastLabel = u32TmpLastLabel;
        *((uint32_t*)psState->au8ConfigOut) = u32TmpConfigOut;
        break;
      case COMM_WR_CFG:
        if (psFlags->bSetCtrlHum) {
          psState->au8ConfigMirror[0] = psState->au8ConfigOut[0];
          psFlags->bSetCtrlHum = false;
        }
        if (psFlags->bSetCtrlMeas) {
          psState->au8ConfigMirror[2] = psState->au8ConfigOut[2];
          psFlags->bSetCtrlMeas = false;
        }
        if (psFlags->bSetConfig) {
          psState->au8ConfigMirror[3] = psState->au8ConfigOut[3];
          psFlags->bSetConfig = false;
        }
        break;
        // getters
      default:
      {
        uint8_t u8FlagMask = 1 << (psFlags->u4UpdAddr - COMM_RD_CALIB0);
        psFlags->u8Getters &= ~u8FlagMask;
      }
    }
    psFlags->u4UpdAddr = 0;
    psFlags->u4UpdFlag = 0;
    bRet = true;
  }
  lockmgr_release_entry(psState->u32LastLabel);
  psFlags->bWaitingForRx = false;
  return bRet;
}

bool bme280_async_tx_cycle(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState) {
  SSyncFlags *psFlags = (SSyncFlags*) & psState->u32CommState;

  // check I.) there is no other communication in progress
  if (psFlags->bWaitingForRx) return false;

  // check II.) anything to do
  bool bWrite = psState->u32CommState & 0x0000000F;
  bool bRead = psState->u32CommState & 0x0000FF00;
  if (!bWrite && !bRead) return false;

  // check III.) I2C ready
  if (!lockmgr_acquire_lock(psIface->eLck, &psState->u32LastLabel)) return false;
  AsyncResultEntry* psEntry = lockmgr_get_entry(psState->u32LastLabel);
  bool bRet = true;

  if (bWrite) {
    if (psFlags->bSetReset) {
      _write_reset(psIface, psState);
    } else {
      _write_cfg(psIface, psState);
    }
  } else if (bRead) {
    ECommAddr eCommAddr = psFlags->bGetCalib0 ? COMM_RD_CALIB0 :
            psFlags->bGetId ? COMM_RD_ID :
            psFlags->bGetCalib1 ? COMM_RD_CALIB1 :
            psFlags->bGetData ? COMM_RD_DATA :
            !psFlags->bGetStatus ? COMM_RD_CFG :
            COMM_RD_STATUS;
    _read_bytes(psIface, psEntry, psState, (uint8_t*) psState + gau8MirrorOffset[eCommAddr], eCommAddr);
  }
  if (bRet) { // kind of exception handling
    psFlags->bWaitingForRx = true;
  } else {
    lockmgr_release_entry(psState->u32LastLabel);
    lockmgr_free_lock(psIface->eLck);
  }
  return bRet;
}
