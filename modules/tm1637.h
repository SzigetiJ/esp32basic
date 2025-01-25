/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef TM1637_H
#define TM1637_H

#ifdef __cplusplus
extern "C" {
#endif

#define TM1637_DATA_LEN 7
#define TM1637_CMDIDX_LEN 3

#include "rmt.h"

  typedef struct {
    uint8_t u8ClkPin;     ///< CLK GPIO pin.
    uint8_t u8DioPin;     ///< DIO GPIO pin.
    ERmtChannel eClkCh;   ///< CLK RMT channel.
    ERmtChannel eDioCh;   ///< DIO RMT channel.
  } STm1637Iface; ///< Describes TM1637 interface.

  typedef struct {
    STm1637Iface sIface;                  ///< TM1637 RMT/GPIO interface.
    uint8_t au8Data[TM1637_DATA_LEN];     ///< Outgoing Command/Data bytes.
    size_t nDataI;                        ///< Current au8Data index.
    size_t anCmdIdx[TM1637_CMDIDX_LEN];   ///< au8Data indices of commands.
    size_t nCmdIdxI;                      ///< Current anCmdIdx index.
    uint32_t u32Internals;                ///< Internal attributes, e.g., ACKs.
    Isr fReadyCb;                         ///< Callback function to invoke when the whole transfer is complete.
    void *pvReadyCbArg;                   ///< Argument to pass to fReadyCb function.
  } STm1637State;


  STm1637State tm1637_config(const STm1637Iface *psIface);
  void tm1637_init(STm1637State *psState, uint32_t u32ApbClkFreq);
  void tm1637_deinit(STm1637State *psState);
  void tm1637_set_data(STm1637State *psState, uint8_t *au8Data);
  void tm1637_set_brightness(STm1637State *psState, bool bOn, uint8_t u8Value);
  void tm1637_set_readycb(STm1637State *psState, Isr fHandler, void *pvArg);
  void tm1637_display(STm1637State *psState);

#ifdef __cplusplus
}
#endif

#endif /* TM1637_H */

