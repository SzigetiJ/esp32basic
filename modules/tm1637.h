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

#define TM1637_MAXCELLS 6      ///< Number of 7 segment cells.
#define TM1637_MAXCOMMANDS 3   ///< Max. number of commands in a communication procedure.

#include "rmt.h"

  typedef struct {
    uint8_t u8ClkPin;     ///< CLK GPIO pin.
    uint8_t u8DioPin;     ///< DIO GPIO pin.
    ERmtChannel eClkCh;   ///< CLK RMT channel.
    ERmtChannel eDioCh;   ///< DIO RMT channel.
  } STm1637Iface; ///< Describes TM1637 interface.

  typedef struct {
    uint8_t u8Begin : 8;
    uint8_t u8End : 8;
    uint8_t u8Cur : 8;
    uint8_t rsvd24 : 8;
  } SRange8Idx; ///< 8 bit Cursor in a range.

  typedef struct {
    STm1637Iface sIface;                  ///< TM1637 RMT/GPIO interface.
    uint8_t *pu8Data;                     ///< Reference to external byte array that contains the data of the 7 segment cells to display
    SRange8Idx sByteI;                     ///< Current au8Bytes index.
    uint8_t au8Bytes[TM1637_MAXCELLS + TM1637_MAXCOMMANDS];     ///< Outgoing Command/Data bytes.
    SRange8Idx sCmdIdxI;                   ///< Current anCmdIdx index.
    uint8_t au8CmdIdx[TM1637_MAXCOMMANDS];   ///< au8Bytes indices of commands.
    uint8_t u8Brightness;
    uint32_t abNak;                       ///< Boolean array of ACK outcomes. Bit_n is 1: Failed ACK.
    Isr fReadyCb;                         ///< Callback function to invoke when the whole transfer is complete.
    void *pvReadyCbArg;                   ///< Argument to pass to fReadyCb function.
  } STm1637State;


  STm1637State tm1637_config(const STm1637Iface *psIface, uint8_t *pu8Data);
  void tm1637_init(STm1637State *psState, uint32_t u32ApbClkFreq);
  void tm1637_deinit(STm1637State *psState);
  void tm1637_set_brightness(STm1637State *psState, bool bOn, uint8_t u8Value);
  void tm1637_set_readycb(STm1637State *psState, Isr fHandler, void *pvArg);
  void tm1637_flush_full(STm1637State *psState, uint8_t u8Len);
  void tm1637_flush_range(STm1637State *psState, uint8_t u8Pos, uint8_t u8Len);
  void tm1637_flush_brightness(STm1637State *psState);

#ifdef __cplusplus
}
#endif

#endif /* TM1637_H */

