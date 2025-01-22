/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef DEFINES_H
#define DEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

  // TIMINGS
  // const -- do not change this value
#define APB_FREQ_HZ         80000000U               // 80 MHz

  // variables
#define TIM0_0_DIVISOR      2U
#define START_APP_CPU       0U
#define SCHEDULE_FREQ_HZ    1000U                  // 1KHz

  // derived invariants
#define CLK_FREQ_HZ         (APB_FREQ_HZ / TIM0_0_DIVISOR)  // 40 MHz
#define TICKS_PER_MS        (CLK_FREQ_HZ / 1000U)           // 40000
#define TICKS_PER_US        (CLK_FREQ_HZ / 1000000U)        // 40
#define NS_PER_TICKS        (1000000000 / CLK_FREQ_HZ)

#define TICKS2NS(X)         ((X) * NS_PER_TICKS)
#define TICKS2US(X)         ((X) / TICKS_PER_US)
#define MS2TICKS(X)         ((X) * TICKS_PER_MS)
#define HZ2APBTICKS(X)      (APB_FREQ_HZ / (X))

#ifdef __cplusplus
}
#endif

#endif /* DEFINES_H */

