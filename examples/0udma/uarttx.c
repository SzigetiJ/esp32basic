/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>

#include "esp_attr.h"
#include "main.h"
#include "defines.h"
#include "dport.h"
#include "romfunctions.h"
#include "timg.h"
#include "uart.h"
#include "utils/uartutils.h"

// =================== Hard constants =================
// #1: Timings
#define UART_FREQ_HZ        115200U
#define CYCLE_PERIOD_MS       1000U

// #2: Sizes
#define TEST_PATTERN_LENGTH   1000U
#define MSG_INC_STEP           100U

// #3: Channels
#define UHCI_INT_CH             23U

// ============= Local types ===============
typedef struct  {
  uint32_t u12Size : 12;
  uint32_t u12Length : 12;
  uint32_t rsvd24: 6;
  uint32_t bEof : 1;
  uint32_t bOwner : 1;
  void *pcData;
  void *psNext;
} UdmaDescriptor;

// ================ Local function declarations =================
static void _pattern_init();
static void _uart_init();
static void _uart_cycle(uint64_t u64tckNow);
static void _uhci_isr(void *pvParam);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// =================== Local constants ================
static const TimerId gsTimer = {.eTimg = TIMG_0, .eTimer = TIMER0};
static const EUartController geUart = UART_CTL0;
static const EUdmaController geUdma = UDMA_CTL0;
static const ECpu eIntCpu = CPU_PRO;

static UART_Type *gpsUART = geUart == UART_CTL0? &gsUART0 : geUart == UART_CTL1 ? &gsUART1 : &gsUART2;
static UHCI_Type *gpsUHCI = geUdma == UDMA_CTL0? &gsUHCI0 : &gsUHCI1;

// ==================== Local Data ================
static uint8_t gu8Phase = 0;

static uint64_t gu64TckUartTxStart;
static uint64_t gu64TckUartTxStop;

static uint64_t gu64TckUdmaTxStart;
static uint64_t gu64TckUdmaTxStop;
static volatile uint64_t gu64TckUdmaTxDone;
static volatile uint64_t gu64TckUdmaTxTotalEof;

static char gacTestPattern[TEST_PATTERN_LENGTH];
static UdmaDescriptor gsUdmaDesc;

// ============== Implementation ==============
// -------------- Internal functions --------------
static void _pattern_init() {
  for (int i = 0; i < TEST_PATTERN_LENGTH; ++i) {
    gacTestPattern[i] = '0' + (i % 10);
  }
}

static void _uart_init() {
  gpsUART->CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);
  uart_init_udma(geUart, geUdma);
  gpsUHCI->INT_ENA = (1 << UHCI_INT_OUTTOTALEOF) | (1 << UHCI_INT_OUTDONE);

  // register ISR and enable it
  RegAddr prDportIntMap = (eIntCpu == CPU_PRO ? &dport_regs()->PRO_UHCI0_INTR_MAP : &dport_regs()->APP_UHCI0_INTR_MAP);
  prDportIntMap += geUdma;

  *prDportIntMap = UHCI_INT_CH;
  _xtos_set_interrupt_handler_arg(UHCI_INT_CH, _uhci_isr, 0);
  ets_isr_unmask(1 << UHCI_INT_CH);

}

static void _uart_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static uint32_t u32MsgLen = MSG_INC_STEP;

  if (u64tckNext <= u64tckNow) {
    switch (gu8Phase) {
      case 0:
        uart_printf(gpsUART, "Sending message of %u bytes\r\n", u32MsgLen);
        break;
      case 1:
        gu64TckUartTxStart = timg_ticks(gsTimer);
        for (int i = 0; i < u32MsgLen; ++i) {
          gpsUART->FIFO = gacTestPattern[i];
        }
        gu64TckUartTxStop = timg_ticks(gsTimer);
        break;
      case 2:
        uart_printf(gpsUART, "\r\nUART TX duration: %u ns\r\n", (uint32_t)((gu64TckUartTxStop - gu64TckUartTxStart) / TICKS_PER_US));
        break;
      case 3:
        gsUdmaDesc = (UdmaDescriptor) {
          .bEof = true,
                  .bOwner = true,
                  .pcData = gacTestPattern,
                  .psNext = NULL,
                  .u12Length = u32MsgLen,
                  .u12Size = TEST_PATTERN_LENGTH
        };
        gpsUHCI->INT_CLR = -1;
        gu64TckUdmaTxStart = timg_ticks(gsTimer);
        gpsUHCI->OUT_LINK = (((uint32_t)&gsUdmaDesc)&0xfffff) | (1 << 29);
        gu64TckUdmaTxStop = timg_ticks(gsTimer);
        break;
      case 4:
        uart_printf(gpsUART, "\r\nUDMA TX duration: %u (send), %u (done), %u (eof) ns)\r\n",
                (uint32_t)((gu64TckUdmaTxStop - gu64TckUdmaTxStart) / TICKS_PER_US),
                (uint32_t)((gu64TckUdmaTxDone - gu64TckUdmaTxStart) / TICKS_PER_US),
                (uint32_t)((gu64TckUdmaTxTotalEof - gu64TckUdmaTxStart) / TICKS_PER_US));
        break;
    }
    ++gu8Phase;
    if (gu8Phase == 5) {
      gu8Phase = 0;
      u32MsgLen+=MSG_INC_STEP;
      if (TEST_PATTERN_LENGTH < u32MsgLen) {
        u32MsgLen = MSG_INC_STEP;
      }
    }
    u64tckNext += MS2TICKS(CYCLE_PERIOD_MS);
  }
}

IRAM_ATTR static void _uhci_isr(void *pvParam) {
  bool bOutDoneEvent = gpsUHCI->INT_ST & (1 << UHCI_INT_OUTDONE);
  bool bOutTotalEofEvent = gpsUHCI->INT_ST & (1 << UHCI_INT_OUTTOTALEOF);
  if (bOutDoneEvent) {
    gu64TckUdmaTxDone = timg_ticks(gsTimer);
    gpsUHCI->INT_CLR = 1 << UHCI_INT_OUTDONE;
  }
  if (bOutTotalEofEvent) {
    gu64TckUdmaTxTotalEof = timg_ticks(gsTimer);
    gpsUHCI->INT_CLR = 1 << UHCI_INT_OUTTOTALEOF;
  }
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  _uart_init();
  _pattern_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _uart_cycle(u64tckNow);
}
