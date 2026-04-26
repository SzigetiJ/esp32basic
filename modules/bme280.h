/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef BME280_H
#define BME280_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/i2ciface.h"

  // ============= Types ===============

  typedef enum {
    BME280_SEL_H = 0,
    BME280_SEL_P,
    BME280_SEL_T
  } EBme280MetricSelector;

  /**
   *  Possible values of osrs_t, osrs_p and osrs_h (oversampling).
   */
  typedef enum {
    BME280_OSRS_SKIP = 0U,
    BME280_OSRS_1 = 1U,
    BME280_OSRS_2 = 2U,
    BME280_OSRS_4 = 3U,
    BME280_OSRS_8 = 4U,
    BME280_OSRS_16 = 5U,
    BME280_OSRS_16B = 6U,
    BME280_OSRS_16C = 7U
  } EBme280Osrs;

  /**
   * Possible values of mode bits in ctrl_meas register
   */
  typedef enum {
    BME280_MODE_SLEEP = 0U,
    BME280_MODE_FORCED = 1U,
    BME280_MODE_FORCED2 = 2U,
    BME280_MODE_NORMAL = 3U
  } EBme280Mode;

    /**
   * Possible values of t_sb (standby time).
   */
  typedef enum {
    BME280_TSB_500US = 0U,
    BME280_TSB_62500US = 1U,
    BME280_TSB_125MS = 2U,
    BME280_TSB_250MS = 3U,
    BME280_TSB_500MS = 4U,
    BME280_TSB_1000MS = 5U,
    BME280_TSB_10MS = 6U,
    BME280_TSB_20MS = 7U
  } EBme280Tsb;

  /**
   * Possible values of filter (iir size).
   */
  typedef enum {
    BME280_IIR_OFF = 0U,
    BME280_IIR_2 = 1U,
    BME280_IIR_4 = 2U,
    BME280_IIR_8 = 3U,
    BME280_IIR_16 = 4U,
    BME280_IIR_16B = 5U,
    BME280_IIR_16C = 6U,
    BME280_IIR_16D = 7U
  } EBme280Iir;

  /**
   * Temperature, pressure and humidity values bound in a structure.
   */
  typedef struct {
    int32_t i32Temp;
    int32_t i32Pres;
    int32_t i32Hum;
  } SBme280TPH;

  /**
   * State descriptor of BME280 stub.
   */
  typedef struct {
    // the following attributes are storing internal state of the asynchronous communication process
    uint32_t u32LastLabel;
    uint32_t u32CommState;
    // the following attributes are storing data trasmitted to / received from the target device
    // memory to send to peripheral
    uint8_t au8ConfigOut[4];      ///< bytes at mem 0xf2 .. 0xf5. Note: 0xf3 (status) is RO, so it is never written
    // memory mirror
    uint8_t au8ConfigMirror[4];   ///< bytes at mem 0xf2 .. 0xf5
    uint8_t au8DataMirror[8];     ///< bytes at mem 0xf7 .. 0xfe
    uint8_t au8CalibMirror[42];   ///< bytes at mem 0x88 .. 0xa1, 0xe1 .. 0xf0
    uint8_t u8IdMirror;           ///< byte at mem 0xd0
  } SBme280StateDesc;

  // ============= Interface functions ===============
  SBme280StateDesc bme280_init_state();

  void bme280_set_osrs(SBme280StateDesc *psState, EBme280MetricSelector eMetric, EBme280Osrs eOsrs);
  void bme280_set_mode(SBme280StateDesc *psState, EBme280Mode eMode);

  EBme280Osrs bme280_get_osrs(const SBme280StateDesc *psState, bool bMirror, EBme280MetricSelector eMetric);
  EBme280Mode bme280_get_mode(const SBme280StateDesc *psState, bool bMirror);

  bool bme280_set_config(SBme280StateDesc *psState, EBme280Tsb eTsb, EBme280Iir eFilter, bool bSpi3wEn);
  EBme280Tsb bme280_get_tsb(const SBme280StateDesc *psState, bool bMirror);
  EBme280Iir bme280_get_filter(const SBme280StateDesc *psState, bool bMirror);
  bool bme280_get_spi3wen(const SBme280StateDesc *psState, bool bMirror);

  void bme280_reset(SBme280StateDesc *psState);
  void bme280_req_id(SBme280StateDesc *psState);
  void bme280_req_config(SBme280StateDesc *psState);
  void bme280_req_status(SBme280StateDesc *psState);
  void bme280_req_calib(SBme280StateDesc *psState);
  void bme280_req_data(SBme280StateDesc *psState);

  bool bme280_is_resetting(const SBme280StateDesc *psState);
  uint8_t bme280_get_id(const SBme280StateDesc *psState);
  uint8_t bme280_get_status(const SBme280StateDesc *psState);
  bool bme280_is_data_ready(const SBme280StateDesc *psState);

  SBme280TPH bme280_get_measurement(const SBme280StateDesc *psState, uint32_t *pu32TFine);
  SBme280TPH bme280_calc_measurement(const uint8_t *pu8Data, const uint8_t *pu8Calib, uint32_t *pu32TFine);

  bool bme280_has_async_todo(const SBme280StateDesc *psState);
  bool bme280_is_waiting(const SBme280StateDesc *psState);
  uint32_t bme280_measurement_duration_hms(const SBme280StateDesc *psState);
  bool bme280_async_rx_cycle(SBme280StateDesc *psState, uint32_t *pu32hmsWaitHint);
  bool bme280_async_tx_cycle(const SI2cIfaceCfg *psIface, SBme280StateDesc *psState);

#ifdef __cplusplus
}
#endif

#endif /* BME280_H */
