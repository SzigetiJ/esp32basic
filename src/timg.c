/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include "dport.h"
#include "romfunctions.h"
#include "timg.h"
#include "xtutils.h"

void timg_callback_dt(TimerId sTimer, uint64_t u64tckDelay, uint8_t u8Int, Isr fCallback, void *pvCallbackParam) {
  uint64_t u64tckTimestamp = timg_ticks(sTimer) + u64tckDelay;
  timg_callback_at(u64tckTimestamp, CPU_PRO, sTimer, u8Int, fCallback, pvCallbackParam);
}

void IRAM_ATTR timg_callback_at(uint64_t u64tckAlarm, ECpu eCpu, TimerId sTimer, uint8_t u8Int, Isr fCallback, void *pvCallbackParam) {
  // TIMG intr
  timg_tregs(sTimer)->ALARMLO = u64tckAlarm & 0xFFFFFFFF;
  timg_tregs(sTimer)->ALARMHI = u64tckAlarm >> 32;
  timg_tregs(sTimer)->CONFIG |= (1 << 10) | (1 << 11); // alarm enabled, generates LVL INT
  gapsTIMG[sTimer.eTimg]->INT_ENA_TIMERS |= 1 << sTimer.eTimer;

  // INTR mx
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_TG_T0_LEVEL_INT_MAP : &dport_regs()->APP_TG_T0_LEVEL_INT_MAP)
          + (sTimer.eTimer == TIMER0 ? 0 : 1)
          + (sTimer.eTimg == TIMG_0 ? 0 : 4);

  *prDportIntMap = u8Int;
  _xtos_set_interrupt_handler_arg(u8Int, fCallback, (int) pvCallbackParam);
  ets_isr_unmask(1 << u8Int);
}
