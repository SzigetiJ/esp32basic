/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "esp_attr.h"
#include "dport.h"
#include "main.h"
#include "pidctrl.h"
#include "romfunctions.h"
#include "rtc.h"
#include "timg.h"
#include "uart.h"
#include "xtutils.h"

extern uint32_t _sbss[0];
extern uint32_t _ebss[0];

static RTC_Type *gpsRTC = &gsRTC;

// ==================== Local constants ====================
static const TimerId gsTimer = {.eTimg = TIMG_0, .eTimer = TIMER0};

// ==================== Local function declarations ====================
static void _init_rtc();
static void _clear_bss();
static void _wait_cycle(TimerId sTimer, uint64_t *pu64tckTarget);
static void _app_main();
static void _start_app_cpu();


// ==================== Local function implementations ====================

static void _init_rtc() {
  // Disable both watchdog timers that are enabled by default on startup.
  // Note that these watchdogs can be protected, but the ROM bootloader
  // doesn't seem to protect them.
  gpsRTC->WDTCONFIG[0] = 0;
  gapsTIMG[0]->WDTCONFIG[0] = 0;

  // Switch SoC clock source to PLL (instead of the default which is XTAl).
  // This switches the CPU (and APB) clock from 40MHz to 80MHz.
  // Bits:
  //   28-27: RTC_CNTL_SOC_CLK_SEL:       1 (default 0)
  //   14-12: RTC_CNTL_CK8M_DIV_SEL:      2 (default)
  //   9:     RTC_CNTL_DIG_CLK8M_D256_EN: 1 (default)
  //   5-4:   RTC_CNTL_CK8M_DIV:          1 (default)
  // The only real change made here is modifying RTC_CNTL_SOC_CLK_SEL, but
  // setting a fixed value produces smaller code.
  gpsRTC->CLK_CONF = (1 << 27) | (2 << 12) | (1 << 9) | (4 << 1);

  // Switch CPU from 80MHz to 160MHz. This doesn't affect the APB clock,
  // which is still running at 80MHz.
  dport_regs()->CPU_PER_CONF = (1 << 0);
}

static void _clear_bss() {
  // Clear the .bss section. The .data section has already been loaded by the
  // ROM bootloader.
  uint32_t *ptr = _sbss;
  while (ptr != _ebss) {
    *ptr = 0;
    ptr++;
  }
}

/**
 *  TODO calc. load statistics
 */
static void _wait_cycle(TimerId sTimer, uint64_t *pu64tckTarget) {
  uint64_t u64tckNow;
  do {
    u64tckNow = timg_ticks(sTimer);
  } while (u64tckNow < *pu64tckTarget);
  *pu64tckTarget += gu64tckSchedulePeriod;
}

static void _start_app_cpu() {
  dport_regs()->APPCPU_CTRL_B = 0;
  dport_regs()->APPCPU_CTRL_A = 1;
  dport_regs()->APPCPU_CTRL_D = (uint32_t) & _app_main;
  dport_regs()->APPCPU_CTRL_A = 0;
  dport_regs()->APPCPU_CTRL_B = 1;
}

/// Main function of the APP CPU

static void IRAM_ATTR _app_main() {
  uint64_t u64CycleCount = 0;
  uint64_t u64tckNow = 0;
  uint64_t u64tckSchedule = 0;

  prog_init_app();

  while (1) {
    u64tckNow = timg_ticks(gsTimer);

    prog_cycle_app(u64tckNow);
    _wait_cycle(gsTimer, &u64tckSchedule);

    ++u64CycleCount;
  }
}

int main(void) {
  // init phase
  _init_rtc();
  _clear_bss();
  timg_init_timer(gsTimer, gu16Tim00Divisor);

  prog_init_pro_pre();
  if (gbStartAppCpu) {
    _start_app_cpu();
  }
  prog_init_pro_post();

  // main cycle
  uint64_t u64CycleCount = 0;
  uint64_t u64tckNow = 0;
  uint64_t u64tckSchedule = 0;
  while (1) {
    u64tckNow = timg_ticks(gsTimer);

    prog_cycle_pro(u64tckNow);
    _wait_cycle(gsTimer, &u64tckSchedule);

    ++u64CycleCount;
  }
}
