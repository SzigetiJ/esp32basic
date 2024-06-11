/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef TIMG_H
#define TIMG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  typedef enum {
    TIMG_0 = 0,
    TIMG_1 = 1
  } ETimG;

  typedef enum {
    TIMER0 = 0,
    TIMER1 = 1
  } ETimer;

  typedef struct {
    ETimG eTimg : 1;
    ETimer eTimer : 1;
  } TimerId;

  // Based on:
  // https://github.com/espressif/esp-idf/blob/6b3da6b188/components/soc/esp32/include/soc/timer_group_reg.h

  typedef struct {
    Reg CONFIG;
    Reg LO;
    Reg HI;
    Reg UPDATE;
    Reg ALARMLO;
    Reg ALARMHI;
    Reg LOADLO;
    Reg LOADHI;
    Reg LOAD;
  } TimerRegs;

  typedef struct {
    TimerRegs T[2];
    Reg WDTCONFIG[6];
    Reg WDTFEED;
    Reg WDTWPROTECT;
    Reg RTCCALICFG;
    Reg RTCCALICFG1;
    Reg LACTCONFIG;
    Reg LACTRTC;
    Reg LACTLO;
    Reg LACTHI;
    Reg LACTUPDATE;
    Reg LACTALARMLO;
    Reg LACTALARMHI;
    Reg LACTLOADLO;
    Reg LACTLOADHI;
    Reg LACTLOAD;
    Reg INT_ENA_TIMERS;
    Reg INT_RAW_TIMERS;
    Reg INT_ST_TIMERS;
    Reg INT_CLR_TIMERS;
  } TIMG_Type;

  extern TIMG_Type gsTIMG0;
  extern TIMG_Type gsTIMG1;
  static TIMG_Type *gapsTIMG[] = {
    &gsTIMG0,
    &gsTIMG1
  };

  static inline TimerRegs *timg_tregs(TimerId sTimer) {
    return &gapsTIMG[sTimer.eTimg]->T[sTimer.eTimer];
  }

  static inline uint64_t timg_ticks(TimerId sTimer) {
    timg_tregs(sTimer)->UPDATE = 0;
    return (uint64_t) (timg_tregs(sTimer)->LO) | ((uint64_t) (timg_tregs(sTimer)->HI) << 32);
  }

  static inline void timg_load(TimerId sTimer, uint64_t u64Value) {
    timg_tregs(sTimer)->LOADLO = (uint32_t) (u64Value & 0xffffffff);
    timg_tregs(sTimer)->LOADHI = (uint32_t) (u64Value >> 32);
    timg_tregs(sTimer)->LOAD = 0;
  }

  static inline void timg_init_timer(TimerId sTimer, uint16_t u16Divisor) {
  timg_tregs(sTimer)->CONFIG = (1 << 31) | (1 << 30) | (u16Divisor << 13);
  timg_load(sTimer, 0ULL);
}

  void timg_callback_dt(TimerId sTimer, uint64_t u64tckDelay, uint8_t u8Int, Isr fCallback, void *pvCallbackParam);
  void timg_callback_at(uint64_t u64tckAlarm, ECpu eCpu, TimerId sTimer, uint8_t u8Int, Isr fCallback, void *pvCallbackParam);

#ifdef __cplusplus
}
#endif

#endif /* TIMG_H */
