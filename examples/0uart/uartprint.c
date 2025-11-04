/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <inttypes.h>

#include "main.h"
#include "defines.h"
#include "uart.h"
#include "utils/uartutils.h"

// =================== Hard constants =================
// #1: Timings
#define UART_FREQ_HZ        115200U
#define SCAN_PERIOD_MS 500U  ///< period of command scan

// ============= Local types ===============

// ================ Local function declarations =================
static void _uart_init();
static void _uart_cycle(uint64_t u64tckNow);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static UART_Type *gpsUART0 = &gsUART0;
static UART_Type *gpsUART0M = &gsUART0Mapped;

// Implementation

static void _uart_init() {
  gpsUART0->CLKDIV.u20ClkDiv = APB_FREQ_HZ / UART_FREQ_HZ;
}

static void _uart_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = 0;
  static char cCommand = ' '; // init value not used.
  static char acArg[4]; // a set of commands require argument(s). They are store in this array (reverse order).
  static uint8_t u8WaitForArgs = 0;  // number of arguments the current command still requires.

  if (u64tckNext <= u64tckNow) {
    while (0 < (gpsUART0->STATUS & 0xff)) {
      char cCtrl = gpsUART0M->FIFO & 0xff;
      if (0 < u8WaitForArgs) {
        acArg[--u8WaitForArgs] = cCtrl;
        if (0 == u8WaitForArgs) { // all the required number of arguments arrived
          switch (cCommand) {
            case 'p':
              uint32_t u32ArgValue = (acArg[0] - '0') + 10 * (acArg[1] - '0') + 100 * (acArg[2] - '0');
              for (uint32_t i = 0; i < u32ArgValue; ++i) {
                gpsUART0->FIFO = ('0' + (i % 10));
              }
              break;
            case 'b':
              uint8_t u8ArgValue = (acArg[0] - '0');

              Reg rUartMemConf = gpsUART0->MEM_CONF;  // read
              rUartMemConf &= ~(0xf << 7);            // erase
              rUartMemConf |= u8ArgValue << 7;        // set
              gpsUART0->MEM_CONF = rUartMemConf;      // write
              uart_printf(gpsUART0, "UART TX buffer size set to %d\r\n", 128 * u8ArgValue);
              break;
          }
        }
      } else {
        switch (cCtrl) {
          case 'h': // help
            uart_printf(gpsUART0, "[h]\tprint help\r\n");
            uart_printf(gpsUART0, "[i]\tprint info\r\n");
            uart_printf(gpsUART0, "[pNNN]\tput NNN bytes into UART TX FIFO\r\n");
            uart_printf(gpsUART0, "[bN]\tset UART TX buffer size to 128 * N\r\n");
            break;
          case 'i':
            uart_printf(gpsUART0, "MEM_CONF: %08X\r\n", gpsUART0->MEM_CONF);
            break;
          case 'p':
            cCommand = cCtrl;
            u8WaitForArgs = 3;
            break;
          case 'b':
            cCommand = cCtrl;
            u8WaitForArgs = 1;
            break;
          default:
            uart_printf(gpsUART0, " `%c' command not recognized\r\n", cCtrl);
        }
      }
    }
    u64tckNext += MS2TICKS(SCAN_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _uart_init();
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
