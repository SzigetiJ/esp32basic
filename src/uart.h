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

  // ============== Defines ==============
#define UART_TCK2CLKDIV(X) (((X) >> 4) & 0xfffff) | (((X) & 0xf) << 20)
  // X: requested UART CLK frequency
  // Y: source CLK frequency (APB or REF)
#define UART_HZ2CLKDIV(X, Y) UART_TCK2CLKDIV(((Y) << 4) / (X))

  // ============== Types ==============

  typedef enum {
    UART_CTL0 = 0,
    UART_CTL1,
    UART_CTL2
  } EUartController;

  typedef enum {
    UDMA_CTL0 = 0,
    UDMA_CTL1
  } EUdmaController;

  typedef enum {
    UHCI_INT_RXSTART = 0,
    UHCI_INT_TXSTART,
    UHCI_INT_RXHUNG,
    UHCI_INT_TXHUNG,
    UHCI_INT_INDONE,  // 4
    UHCI_INT_INSUCEOF,
    UHCI_INT_INERREOF,
    UHCI_INT_OUTDONE,
    UHCI_INT_OUTEOF,  // 8
    UHCI_INT_INDSCRERR,
    UHCI_INT_OUTDSCRERR,
    UHCI_INT_INDSCREMPTY,
    UHCI_INT_OUTLINKEOFERR, // 12
    UHCI_INT_OUTTOTALEOF,
    UHCI_INT_SENDSREGQ,
    UHCI_INT_SENDAREGQ,
    UHCI_INT_DMAINFIFOFULLWM  // 16
  } EUhciIntType;        ///< Types of UHCI interrupt.

  typedef struct  {
    uint32_t u12Size : 12;
    uint32_t u12Length : 12;
    uint32_t rsvd24 : 6;
    uint32_t bEof : 1;
    uint32_t bOwner : 1;
    void *pcData;
    void *psNext;
  } UdmaDescriptor;


  // Based on:
  // https://github.com/espressif/esp-idf/blob/6b3da6b188/components/soc/esp32/include/soc/uart_reg.h

  typedef volatile union {

    volatile struct {
      uint32_t u20ClkDiv : 20;   ///< Integral part of the divisor
      uint32_t u4ClkDivFrag : 4;  ///< Decimal part of the divisor
      uint32_t rsvd24 : 8;
    } ;
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
    Reg STATUS; // 0..7: RX FIFO CNT, [...],  16..23: TX FIFO CNT
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
    Reg MEM_CNT_STATUS; // 0..2: RX, 3..5: TX
    Reg POSPULSE;
    Reg NEGPULSE;
  } UART_Type;

  typedef struct {
    Reg CONF0;      // RW
    Reg INT_RAW;    // RO
    Reg INT_ST;     // RO
    Reg INT_ENA;    // RW
    Reg INT_CLR;    // 0x10 WO
    Reg OUT_STATUS; // RO
    Reg OUT_PUSH;   // RW
    Reg RESV1C;     //
    Reg IN_POP;     // 0x20 RO
    Reg OUT_LINK;   // RW
    Reg IN_LINK;    // RW
    Reg CONF1;      // RW
    Reg RESV30;     // 0x30
    Reg RESV34;
    Reg OUT_EOF_DESC_ADDR; // RO
    Reg IN_SUC_EOF_DESC_ADDR; // RO
    Reg IN_ERR_EOF_DESC_ADDR; // 0x40 RO
    Reg OUT_ERR_EOF_DESC_ADDR; // RO
    Reg RESV48;
    Reg IN_CURR_DESC0;  // RO
    Reg IN_CURR_DESC1;  // 0x50 RO
    Reg IN_CURR_DESC2;  // RO
    Reg OUT_CURR_DESC0; // RO
    Reg OUT_CURR_DESC1; // RO
    Reg OUT_CURR_DESC2; // 0x60 RO
    Reg ESC_CHAR_CONF;  // RW
    Reg HUNG_CONF;      // RW
    Reg RESV6C;
    Reg RESV70[16];
    Reg ESC_SEQ_CONF[4];  // RW
  } UHCI_Type;

  // ============== Global values / References ==============
  extern UART_Type gsUART0;
  extern UART_Type gsUART1;
  extern UART_Type gsUART2;

  extern UART_Type gsUART0Mapped;
  extern UART_Type gsUART1Mapped;
  extern UART_Type gsUART2Mapped;

  extern UHCI_Type gsUHCI0;
  extern UHCI_Type gsUHCI1;

  // ============== Inline interface functions ==============

  static inline uint32_t uart_tx_cnt(UART_Type *psUart) {
    return ((psUart->STATUS >> 16) & 0xff) | (((psUart->MEM_CNT_STATUS >> 3) & 0x7) << 8);
  }

  // ============== Interface functions ==============
  void uart_init_udma(EUartController eUart, EUdmaController eUdma);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
