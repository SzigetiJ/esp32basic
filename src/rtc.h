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

  typedef enum {
    RTCCLKSRC_XTL = 0,
    RTCCLKSRC_PLL = 1,
    RTCCLKSRC_RCFAST = 2,
    RTCCLKSRC_APLL = 3
  } ERtcClkSrc;

/*  typedef volatile union {

    volatile struct {
      uint32_t rsvd0 : 4;
      uint32_t u2Ck8mDiv : 2;
      uint32_t bEnbCk8m : 1;
      uint32_t bEnbCk8mDiv : 1;
      uint32_t bDigXtal32kEn : 1; // b8
      uint32_t bDigClk8mD256En : 1;
      uint32_t bDigClk8mEn : 1;
      uint32_t rsvd11 : 1;
      uint32_t u3Ck8mDivSel : 3;
      uint32_t rsvd15 : 2;
      uint32_t u8Ck8mDfreq : 8;
      uint32_t bCk8mForcePd : 1;  // b25
      uint32_t bCk8mForcePu : 1;
      uint32_t e2SocClkSel : 2;   // b27
      uint32_t RtcFastClkSel : 1;
      uint32_t u2AnaClkRtcSel : 2;  // b30
    } ;
    volatile uint32_t raw;
  } SRtcClkConf;
*/
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
