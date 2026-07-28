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


  /**
   * Initializes a state object.
   * @param psIface Defines TM1637 communication interface (GPIO pins, RMT channels).
   * @param pu8Data Source of 7 segment characters to display.
   * @return Initialized TM1637 state descriptor.
   */
  STm1637State tm1637_config(const STm1637Iface *psIface, uint8_t *pu8Data);

  /**
   * Initializes TM1637 communication peripherals.
   * @param psState TM1637 state descriptor.
   * @param u32ApbClkFreq APB clock frequency, used for calculating RMT divisor (and implicitly TM1637 CLK frequency).
   * The default TM1637 CLK frequency is 500KHz. If the real APB clk freq is 80.000.000, but u32ApbClkFreq is set to 40.000.000,
   * the TM1637 CLK frequency will be 1MHz, etc. The TM1637 CLK frequency can be tuned up to 2.5MHz (experimental).
   */
  void tm1637_init(STm1637State *psState, uint32_t u32ApbClkFreq);
  void tm1637_deinit(STm1637State *psState);
  
  /**
   * Sets TM1637 brightness value.
   * Note, this function does not trigger communication. Either tm1637_flush_full()
   * or tm1637_flush_brightness() must be invoked in order to apply changes.
   * @param psState TM1637 state descriptor.
   * @param bOn Turn display on (false: turn off).
   * @param u8Value Valid range: [0..7] 0: lowest (1/16), 7: highest (14/16) brightness.
   */
  void tm1637_set_brightness(STm1637State *psState, bool bOn, uint8_t u8Value);

  /**
   * Set callback function and parameter.
   * This callback function will be invoked when any communication process is over.
   * @param psState TM1637 state descriptor.
   * @param fHandler Callback function.
   * @param pvArg Parameter of the callback function.
   */
  void tm1637_set_readycb(STm1637State *psState, Isr fHandler, void *pvArg);

  /**
   * Full flush consists of 3 TM1637 commands:
   * 1. CMD_SETDATA: set write mode (with incremental addressing).
   * 2. CMD_SETADDRESS: set initial address (and write data to display registers).
   * 3. CMD_CTRLDISPLAY: set brightness.
   * @param psState TM1637 state descriptor.
   * @param u8Len Number of cells/characters to update in the display registers.
   */
  void tm1637_flush_full(STm1637State *psState, uint8_t u8Len);

  /**
   * Limited display register update.
   * A single command is sent (CMD_SETADDRESS) followed by data bytes.
   * @param psState TM1637 state descriptor.
   * @param u8Pos Start cell/character update at this position.
   * @param u8Len Number of data bytes to send (number of cells/characters to update).
   */

  void tm1637_flush_range(STm1637State *psState, uint8_t u8Pos, uint8_t u8Len);
  /**
   * Sets brightness on the TM1637 device.
   * A single command is sent (CMD_CTRLDISPLAY) without any data bytes.
   * tm1637_set_brightness() must be called before this function.
   * @param psState TM1637 state descriptor.
   */
  void tm1637_flush_brightness(STm1637State *psState);

#ifdef __cplusplus
}
#endif

#endif /* TM1637_H */

