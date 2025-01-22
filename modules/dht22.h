/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rmt.h"

#define DHT22_DATA_LEN 5

  // ============= Types ===============

  /**
   * This struct contains bit decoded DHT22 data.
   * The raw data (signal level and duration) is not stored.
   */
  typedef struct {
    uint8_t au8Data[DHT22_DATA_LEN];    ///< Stores raw data decoded bit-by-bit from RMT RAM entries.
    uint8_t au8Invalid[DHT22_DATA_LEN]; ///< Shows which bits could not be decoded from RMT RAM entries.
  } SDht22Data;

  /**
   * Function type of the callback that gets invoked,
   * when the DHT22 data is received and bit decoded.
   * pvParam is Arbitrary data passed to the function,
   * whereas psData contains the result of the bit-decoding.
   */
  typedef void (*FDht22Callback)(void *pvParam, SDht22Data *psData);

  /**
   * Contains data that the interface functions require.
   */
  typedef struct {
    ERmtChannel eChannel;     ///< RMT channel.
    FDht22Callback fReadyCb;  ///< Callback to invoke when all the data is received and decoded.
    void *pvReadyCbParam;     ///< First parameter to pass to the callback function.
    SDht22Data sData;         ///< Store decoded sensor data.
  } SDht22Descriptor;

  // ============= Inline functions ===============

  /**
   * Creates and initializes SDht22Descriptor instance.
   * @param eChannel  Identifies the RMT channel.
   * @param fReadyCb  Callback to invoke when the data is received and decoded.
   * @param pvReadyCbParam  First parameter to pass to the callback.
   * @return  Initialized descriptor instance.
   */
  static inline SDht22Descriptor dht22_config(ERmtChannel eChannel, FDht22Callback fReadyCb, void *pvReadyCbParam) {
    SDht22Descriptor sRet = {.eChannel = eChannel, .fReadyCb = fReadyCb, .pvReadyCbParam = pvReadyCbParam};
    return sRet;
  }


  // ============= Interface function declaration ===============
  void dht22_init(uint8_t u8Pin, uint32_t u32ApbClkFreq, SDht22Descriptor *psDht22Desc);
  void dht22_run(SDht22Descriptor *psDht22Desc);
  uint16_t dht22_get_rhum(const SDht22Data *psData);
  int16_t dht22_get_temp(const SDht22Data *psData);
  bool dht22_data_valid(const SDht22Data *psData);


#ifdef __cplusplus
}
#endif

#endif /* DHT22_H */

