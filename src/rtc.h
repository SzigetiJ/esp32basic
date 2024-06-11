/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef RTC_H
#define RTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  // Based on:
  // https://github.com/espressif/esp-idf/blob/6b3da6b188/components/soc/esp32/include/soc/rtc_reg.h

  typedef struct {
    Reg OPTIONS0;
    Reg SLP_TIMER0;
    Reg SLP_TIMER1;
    Reg TIME_UPDATE;
    Reg TIME0;
    Reg TIME1;
    Reg STATE0;
    Reg TIMER1;
    Reg TIMER2;
    Reg TIMER3; // not present in datasheet?
    Reg TIMER4; // not present in datasheet?
    Reg TIMER5;
    Reg ANA_CONF;
    Reg RESET_STATE;
    Reg WAKEUP_STATE;
    Reg INT_ENA;
    Reg INT_RAW;
    Reg INT_ST;
    Reg INT_CLR;
    Reg STORE0;
    Reg STORE1;
    Reg STORE2;
    Reg STORE3;
    Reg EXT_XTL_CONF;
    Reg EXT_WAKEUP_CONF;
    Reg SLP_REJECT_CONF;
    Reg CPU_PERIOD_CONF;
    Reg SDIO_ACT_CONF;
    Reg CLK_CONF;
    Reg SDIO_CONF;
    Reg BIAS_CONF;
    Reg VREG;
    Reg PWC;
    Reg DIG_PWC;
    Reg DIG_ISO;
    Reg WDTCONFIG[5];
    Reg WDTFEED;
    Reg WDTWPROTECT;
    Reg TEST_MUX;
    Reg SW_CPU_STALL;
    Reg STORE4;
    Reg STORE5;
    Reg STORE6;
    Reg STORE7;
    Reg LOW_POWER_ST;
    Reg DIAG1;
    Reg HOLD_FORCE;
    Reg EXT_WAKEUP1;
    Reg EXT_WAKEUP1_STATUS;
    Reg BROWN_OUT;
  } RTC_Type;

  extern RTC_Type gsRTC;

#ifdef __cplusplus
}
#endif

#endif /* RTC_H */
