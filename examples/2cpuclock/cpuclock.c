/*
 * Copyright 2024 - 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "clock.h"
#include "esp_attr.h"
#include "dport.h"
#include "gpio.h"
#include "iomux.h"
#include "main.h"
#include "defines.h"
#include "romfunctions.h"
#include "rtc.h"
#include "typeaux.h"
#include "uart.h"
#include "utils/uartutils.h"
#include "timg.h"

// =================== Hard constants =================
// #0: Timings
#define UART_PERIOD_MS       100U     ///< for polling UART0 RX for incoming commands
#define LED_PERIOD_MS       1000U
#define LED_HIGH_MS          100U

#define REF_TICK_FREQ_HZ 1000000U
#define XTL_CLK_FREQ_HZ 40000000U
// derived
#define XTL_CLK_FREQ_MHZ (XTL_CLK_FREQ_HZ / 1000000U)
#define APB_FREQ_MHZ (APB_FREQ_HZ / 1000000U)
#define TIMG00_XTLBASE_FREQ_KHZ (XTL_CLK_FREQ_HZ / (TIM0_0_DIVISOR * 1000))

// #1: Limits

// #2: Connections
#define LED_GPIO 2U

// #3: Sizes

// ============= Local types ===============


// ================ Local function declarations =================
static uint32_t _ms_to_tick(uint32_t u32msValue);
static void _led_init();
static void _led_cycle(uint64_t u64tckNow);
static void _switch_to_any_clk(ECpuClockSource eClk);
static void _uart0_autoconf();
static void _uart_config(bool bApbBased, bool bPllCpuClkSrc, uint32_t u32Baud);
static void _uart_init();
static void _uart_cycle(uint64_t u64tckNow);
static void _vregdbias_init();
static uint8_t _xtal_ref_tick_div(uint8_t u8Idx);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
/// Predefined set of XTL_CLK divisors
const static uint32_t gau32XtalDiv[] = {
  1, 2, 4, 5, 8, 10
};
static uint8_t gu8XtalClkIdx = 0;
static EPllCpuClockFreq gePllCpuClkFreq = PLLCPUFREQ_160MHZ;
static ECpuClockSource geCpuClkSrc = CLK_PLL;

/// Predefined UART baud rates
const static uint32_t gau32UartBaud[] = {
  9600, 19200, 38400, 57600, 115200, 230400, 460800
};
static uint8_t gu8UartBaudIdx = 4;

/// Possible CPU voltage levels (mV)
const static uint32_t gau32Vreg[] = {
  900, 950, 1000, 1050, 1100, 1150, 1200, 1250
};

static bool gbUart0ApbBased = true;

static uint8_t gau8XtalVregDBiasUser[ARRAY_SIZE(gau32XtalDiv)];
static uint8_t gau8PllVregDBiasUser[3];
static bool gbLoadVregDBias;

// ============== Implementation ==============
// -------------- Internal functions --------------

static uint32_t _ms_to_tick(uint32_t u32msValue) {
  if (geCpuClkSrc == CLK_PLL) return MS2TICKS(u32msValue);
  return (TIMG00_XTLBASE_FREQ_KHZ / gau32XtalDiv[gu8XtalClkIdx]) * u32msValue;
}

static void _led_init() {
  gpio_pin_enable(LED_GPIO);
}

static void _led_cycle(uint64_t u64tckNow) {
  static const RegAddr aprGpioOut[] = {&gsGPIO.OUT_W1TC, &gsGPIO.OUT_W1TS};
  static uint64_t u64tckNext = 0;
  static bool gbLedOn = false;

  if (u64tckNext <= u64tckNow) {
    gbLedOn = !gbLedOn;
    gpio_reg_setbit(aprGpioOut[gbLedOn], LED_GPIO);
    u64tckNext += _ms_to_tick(gbLedOn ? LED_HIGH_MS : (LED_PERIOD_MS - LED_HIGH_MS));
  }
}

static void _uart_config(bool bApbBased, bool bPllCpuClkSrc, uint32_t u32Baud) {
  gsUART0.CONF0 = FIELD_REPLACE(gsUART0.CONF0, bApbBased ? 1 : 0, UART_TICK_REF_ALWAYS_ON);
  uint32_t u32ClkFreqHz =  bApbBased ? (bPllCpuClkSrc ? APB_FREQ_HZ : XTL_CLK_FREQ_HZ / gau32XtalDiv[gu8XtalClkIdx]) : REF_TICK_FREQ_HZ;
  gsUART0.CLKDIV.raw = UART_HZ2CLKDIV(u32Baud, u32ClkFreqHz);
}

static void _uart0_autoconf() {
  _uart_config(gbUart0ApbBased, geCpuClkSrc == CLK_PLL, gau32UartBaud[gu8UartBaudIdx]);
}

static void _uart_init() {
  _uart0_autoconf();
}

static void _vregdbias_init() {
  for (int i = 0; i < ARRAY_SIZE(gau8PllVregDBiasUser); ++i) {
    gau8PllVregDBiasUser[i] = -1; // not set yet
  }
  for (int i = 0; i < ARRAY_SIZE(gau8XtalVregDBiasUser); ++i) {
    gau8XtalVregDBiasUser[i] = -1; // not set yet
  }
  gbLoadVregDBias = true;
}

static uint8_t _xtal_ref_tick_div(uint8_t u8Idx) {
  return (XTL_CLK_FREQ_HZ / REF_TICK_FREQ_HZ) / gau32XtalDiv[u8Idx];
};

static inline void _print_xtl_params() {
  uint32_t u32CpuFreqKHz = (XTL_CLK_FREQ_HZ / 1000) / gau32XtalDiv[gu8XtalClkIdx];
  uint32_t u32RefFreqKHz = u32CpuFreqKHz / _xtal_ref_tick_div(gu8XtalClkIdx);
  uart_printf(&gsUART0, "XTAL div#%u: %u, %u: %uKHz (ref: %uKHz)\r\n",
          gu8XtalClkIdx, gau32XtalDiv[gu8XtalClkIdx],  _xtal_ref_tick_div(gu8XtalClkIdx),
          u32CpuFreqKHz, u32RefFreqKHz);
}

static bool _modify_idx(uint8_t *pu8Idx, int8_t i8Diff, uint8_t u8Size, const uint32_t *pu32Values, const char *strText) {
  bool bRet = false;
  uint8_t u8IdxOrig = *pu8Idx;
  if (i8Diff < 0) {
    if (-i8Diff < *pu8Idx) {
      *pu8Idx += i8Diff;
    } else {
      *pu8Idx = 0;
    }
  } else {
    if (*pu8Idx + i8Diff < u8Size) {
      *pu8Idx += i8Diff;
    } else {
      *pu8Idx = u8Size - 1;
    }
  }
  if (*pu8Idx != u8IdxOrig) {
    if (pu32Values) {
      uart_printf(&gsUART0, "%s changed: %u -> %u\r\n", strText, pu32Values[u8IdxOrig], pu32Values[*pu8Idx]);
    } else {
      uart_printf(&gsUART0, "%s idx changed: %u -> %u\r\n", strText, u8IdxOrig, *pu8Idx);
    }
    bRet = true;
  } else {
    if (pu32Values) {
      uart_printf(&gsUART0, "%s (%u) not changed\r\n", strText, pu32Values[u8IdxOrig]);
    } else {
      uart_printf(&gsUART0, "%s idx (%u) not changed\r\n", strText, u8IdxOrig);
    }
  }
  return bRet;
}

static void _switch_to_any_clk(ECpuClockSource eClk) {
  ECpuClockSource eCpuClkSrcOrig = geCpuClkSrc;
  if (eClk == CLK_XTL) {
    clock_switch_to_xtl(gau32XtalDiv[gu8XtalClkIdx],  _xtal_ref_tick_div(gu8XtalClkIdx));
  } else if (eClk == CLK_PLL) {
    clock_switch_to_pll(gePllCpuClkFreq);
  }
  geCpuClkSrc = eClk;
  uint8_t u8FreqIdx = ((eClk == CLK_XTL) ? gu8XtalClkIdx : gePllCpuClkFreq);
  uint8_t *pu8VregDBiasUser = ((eClk == CLK_XTL) ? gau8XtalVregDBiasUser : gau8PllVregDBiasUser);

  if (gbLoadVregDBias && pu8VregDBiasUser[u8FreqIdx] < 8) {
    clock_set_dig_vreg_dbias_wak(pu8VregDBiasUser[u8FreqIdx]);
    uart_printf(&gsUART0, "Dig Vreg DBias set to %u mV\r\n", gau32Vreg[pu8VregDBiasUser[u8FreqIdx]]);
  }
  // resolve UART baud issue in case of APB freq change
  if (gbUart0ApbBased && !(eCpuClkSrcOrig == CLK_PLL && eClk == CLK_PLL)) {
    _uart0_autoconf();
  }
}

static void _uart_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = MS2TICKS(1000);

  if (u64tckNext <= u64tckNow) {
    while (0 < (gsUART0.STATUS & 0xff)) {
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      int8_t i8UartBaudDiff = 0;
      int8_t i8XtlDivDiff = 0;
      int8_t i8PllFreqSelDiff = 0;
      int8_t i8DigVregDBiasDiff = 0;

      switch (cCtrl) {
        case '-':
          --i8XtlDivDiff;
          break;
        case '+':
          ++i8XtlDivDiff;
          break;

        case 'r':
          gbLoadVregDBias = true;
          uart_printf(&gsUART0, "Load stored vreg dbias when CPU CLK is changed.\r\n");
          break;
        case 'R':
          gbLoadVregDBias = false;
          uart_printf(&gsUART0, "Load default vreg dbias when CPU CLK is changed.\r\n");
          break;

          // modify UART0 baud
        case 'u':
          --i8UartBaudDiff;
          break;
        case 'U':
          ++i8UartBaudDiff;
          break;
          // modify PLL-based CPU freq
        case '[':
          --i8PllFreqSelDiff;
          break;
        case ']':
          ++i8PllFreqSelDiff;
          break;
          // toggle UART0 clock source
        case 'a':
          gbUart0ApbBased = !gbUart0ApbBased;
          _uart0_autoconf();
          uart_printf(&gsUART0, "UART0 based on %s\r\n", gbUart0ApbBased ? "APB_CLK" : "REF_TICK");
          break;
          // print register information
        case 'i':
          uart_printf(&gsUART0, "UART\tCONF0:  %08X\tAUTOBAUD: %08X\tCLKDIV: %08X\r\n", gsUART0.CONF0, gsUART0.AUTOBAUD, gsUART0.CLKDIV);
          uart_printf(&gsUART0, "SYSCON\tSYSCLK: %08X\tXTALTICK: %08X\tPLLTICK: %08X\r\n", gprSYSCON[0], gprSYSCON[1], gprSYSCON[2]);
          break;
        case 'I':
          uart_printf(&gsUART0, "ClkSrc: %u, XTL freq: #%u (%u MHz), PLL freq: #%u (%u MHz), UART_APB: %c, LoadDBias: %c, dbias: #%u\r\n",
                  geCpuClkSrc,
                  gu8XtalClkIdx, XTL_CLK_FREQ_MHZ / gau32XtalDiv[gu8XtalClkIdx],
                  gePllCpuClkFreq, APB_FREQ_MHZ * (1 + gePllCpuClkFreq),
                  gbUart0ApbBased ? 'Y' : 'N', gbLoadVregDBias ? 'Y' : 'N', clock_get_dig_vreg_dbias_wak());
          break;
        case 'x': // switch to xtal
        {
          uart_printf(&gsUART0, "Switch to XTL CPU clock source...");
          _switch_to_any_clk(CLK_XTL);
          uart_printf(&gsUART0, "done.\r\n");
        }
          break;
        case 'p': // switch to xtal
        {
          uart_printf(&gsUART0, "Switch to PLL CPU clock source...");
          _switch_to_any_clk(CLK_PLL);
          uart_printf(&gsUART0, "done.\r\n");
        }
          break;

        case '<':
          --i8DigVregDBiasDiff;
          break;
        case '>':
          ++i8DigVregDBiasDiff;
          break;
        default:
          uart_printf(&gsUART0, "_ _ _ [%c] _ _ _\r\n", cCtrl);
      }

      // change uart0 baud
      if (i8UartBaudDiff != 0) {
        if (_modify_idx(&gu8UartBaudIdx, i8UartBaudDiff, ARRAY_SIZE(gau32UartBaud), gau32UartBaud, "UART0 baud")) {
          _uart0_autoconf();
        }
      }

      // set XTL divisor
      if (i8XtlDivDiff != 0) {
        if (_modify_idx(&gu8XtalClkIdx, i8XtlDivDiff, ARRAY_SIZE(gau32XtalDiv), gau32XtalDiv, "XTAL DIV")) {
          if (geCpuClkSrc == CLK_XTL) {
            _switch_to_any_clk(CLK_XTL);
          } else {
            uart_printf(&gsUART0, "But CPU clock not changed as current clock source is not XTAL.\r\n");
          }
        }
      }

      // set PLL frequency
      if (i8PllFreqSelDiff != 0) {
        uint8_t u8PllFreqSel = gePllCpuClkFreq;
        if (_modify_idx(&u8PllFreqSel, i8PllFreqSelDiff, 3, NULL, "PLL Freq. selector")) {
          gePllCpuClkFreq = u8PllFreqSel;
          if (geCpuClkSrc == CLK_PLL) {
            _switch_to_any_clk(CLK_PLL);
          } else {
            uart_printf(&gsUART0, "But CPU clock not changed as current clock source is not PLL.\r\n");
          }
        }
      }

      // set digital voltage regulator dbias
      if (i8DigVregDBiasDiff != 0) {
        uint8_t u8dbias = clock_get_dig_vreg_dbias_wak();
        if (_modify_idx(&u8dbias, i8DigVregDBiasDiff, ARRAY_SIZE(gau32Vreg), gau32Vreg, "dig vreg dbias [mV]")) {
          clock_set_dig_vreg_dbias_wak(u8dbias);
          uart_printf(&gsUART0, "DBIAS: %u\r\n", clock_get_dig_vreg_dbias_wak());
        } else {
          uart_printf(&gsUART0, "Dig vreg dbias not modified\r\n");
        }
        if (geCpuClkSrc == CLK_XTL) {
          gau8XtalVregDBiasUser[gu8XtalClkIdx] = u8dbias;
        } else if (geCpuClkSrc == CLK_PLL) {
          gau8PllVregDBiasUser[gePllCpuClkFreq] = u8dbias;
        }
      }
    }

    u64tckNext += _ms_to_tick(UART_PERIOD_MS);
  }
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  _led_init();
  _uart_init();
  _vregdbias_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _led_cycle(u64tckNow);
  _uart_cycle(u64tckNow);
}
