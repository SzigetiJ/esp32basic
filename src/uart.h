/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef UART_H
#define UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  // Based on:
  // https://github.com/espressif/esp-idf/blob/6b3da6b188/components/soc/esp32/include/soc/uart_reg.h

  typedef volatile union {

    volatile struct {
      uint32_t u20ClkDiv : 20;   ///< Integral part of the divisor
      uint32_t u4ClkDivFrag : 4;  ///< Decimal part of the divisor
      uint32_t rsvd24 : 8;
    };
    volatile uint32_t raw;
  } SUartDivReg;

  typedef struct {
    Reg FIFO; // 0..7
    Reg INT_RAW;
    Reg INT_ST;
    Reg INT_ENA;
    Reg INT_CLR; // 0x10
    SUartDivReg CLKDIV;
    Reg AUTOBAUD;
    Reg STATUS;
    Reg CONF0; // 0x20
    Reg CONF1;
    Reg LOWPULSE;
    Reg HIGHPULSE;
    Reg RXD_CNT; // 0x30
    Reg FLOW_CONF;
    Reg SLEEP_CONF;
    Reg SWFC_CONF;
    Reg IDLE_CONF; // 0x40
    Reg RS485_CONF;
    Reg AT_CMD_PRECNT;
    Reg AT_CMD_POSTCNT;
    Reg AT_CMD_GAPTOUT; // 0x50
    Reg AT_CMD_CHAR;
    Reg MEM_CONF;
    Reg MEM_TX_STATUS; // 2..12: RD (USR-TO-BUF) PTR, 13..23: WR (BUF-TO-PERI) PTR
    Reg MEM_RX_STATUS; // 0x60
    Reg MEM_CNT_STATUS;
    Reg POSPULSE;
    Reg NEGPULSE;
  } UART_Type;

  extern UART_Type gsUART0;
  extern UART_Type gsUART1;
  extern UART_Type gsUART2;

  extern UART_Type gsUART0Mapped;
  extern UART_Type gsUART1Mapped;
  extern UART_Type gsUART2Mapped;

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
