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

  typedef struct {
    uint8_t au8Data[DHT22_DATA_LEN];
    uint8_t au8Invalid[DHT22_DATA_LEN];
  } SDht22Data;

  typedef void (*FDht22Callback)(void *pvParam, SDht22Data *psData);

  typedef struct {
    ERmtChannel eChannel;
    FDht22Callback fReadyCb;
    void *pvReadyCbParam;
    SDht22Data sData;
  } SDht22Descriptor;

  // ============= Inline functions ===============

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

