/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include "dport.h"
#include "uart.h"

// ============== Defines ==============
#define DPORT_UHCI0_BIT 8U
#define DPORT_UHCI1_BIT 12U

// ============== Local types ==============
typedef struct  {
  uint32_t u12Size : 12;
  uint32_t u12Length : 12;
  uint32_t rsvd24: 6;
  uint32_t bEof : 1;
  uint32_t bOwner : 1;
  void *pcData;
  void *psNext;
} UdmaDescriptor;

// ============== Internal function declarations ==============
static inline uint8_t _dport_peri_bit(EUdmaController eUdma);

// ============== Implementation ==============
// -------------- Internal functions --------------

/**
 * Get DPORT_PERIP_{CLK|RST}_EN_REG bit of the given UDMA controller.
 * @param eUdma UDMA controller
 * @return Bit
 */
static inline uint8_t _dport_peri_bit(EUdmaController eUdma) {
  return eUdma == UDMA_CTL0 ? DPORT_UHCI0_BIT : DPORT_UHCI1_BIT;
}


// -------------- Interface functions --------------

void uart_init_udma(EUartController eUart, EUdmaController eUdma) {
  // enable controller
  dport_regs()->PERIP_CLK_EN |= 1 << _dport_peri_bit(eUdma);
  dport_regs()->PERIP_RST_EN |= 1 << _dport_peri_bit(eUdma);
  dport_regs()->PERIP_RST_EN &= ~(1 << _dport_peri_bit(eUdma));

  // assign uart ctrl to udma ctrl
  gsUHCI0.CONF0 = 1 << (9 + (uint8_t)eUart);
  gsUHCI0.CONF1 = 0;
}
