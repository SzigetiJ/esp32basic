/*
 * Copyright 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef CLOCK_H
#define CLOCK_H

#include "esp32types.h"

/**
 * @file
 * Clock-related types and functions.
 * Note, not to confuse with General-purpose Timers, @ref timg.h "TIMG"!
 * @see timg.h
 */
#ifdef __cplusplus
extern "C" {
#endif

  // ============== Defines ==============


  // ============== Types ==============

  typedef enum {
    CLK_XTL = 0,
    CLK_PLL,
    CLK_RC_FAST,
    CLK_APLL
  } ECpuClockSource; ///< Clock source of the CPU.

  typedef enum {
    PLLCPUFREQ_80MHZ = 0,
    PLLCPUFREQ_160MHZ,
    PLLCPUFREQ_240MHZ,
    PLLCPUFREQ_INVALID
  } EPllCpuClockFreq;  ///< PLL based CPU clock frequencies.


  // ============== Global values / References ==============
  extern Reg gprSYSCON[32];

  // ============== Inline interface functions ==============

  // ============== Interface functions ==============
  uint8_t clock_get_dig_vreg_dbias_wak();
  void clock_set_dig_vreg_dbias_wak(uint8_t u8DigDbias);
  ECpuClockSource clock_get_cpuclksrc();
  EPllCpuClockFreq clock_get_pllcpufreq();
  void clock_switch_to_xtl(uint16_t u16Divisor, uint8_t u8RefDivisor);
  void clock_switch_to_pll(EPllCpuClockFreq eFreq);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_H */

