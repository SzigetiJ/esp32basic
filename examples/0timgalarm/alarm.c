/*
 * Copyright 2024 - 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "esp_attr.h"
#include "dport.h"
#include "gpio.h"
#include "iomux.h"
#include "main.h"
#include "defines.h"
#include "romfunctions.h"
#include "typeaux.h"
#include "uart.h"
#include "utils/uartutils.h"
#include "timg.h"

// =================== Hard constants =================
// #0: timings
#define UART_PERIOD_MS       100U     ///< for polling UART0 RX for incoming commands
#define UART_FLUSH_PERIOD_MS  10U     ///< for flushing out multiple lines to UART0
#define UART_FREQ_HZ      115200U
#define CONST_ALARM_DELAY_TCK  3U     ///< experiments shows that periodic alarm using autoreload introduces some constant alarm delay

// #1: limits
#define ALARM_DIVISOR TIM0_0_DIVISOR
#define ALARM_VAL_MIN         50U
#define ALARM_VAL_MAX      10000U
#define ALARM_VAL_INIT       200U
#define SAMPLE_MIN            10U
#define SAMPLE_MAX           200U
#define SAMPLE_INIT           50U

// #2: Channels
#define INT_CH        24U

// #3: Sizes
#define SAMPLE_SIZE   SAMPLE_MAX

// ============= Local types ===============

/// The results of the last measurement are stored in such structure.

typedef struct {
  bool bReload;     ///< true: autoreload, false: alarm incrementation
  TimerId sAlarm;   ///< timer used for the measurement
  uint32_t u32tckPeriod;
  uint32_t u32SampleLen;
  uint64_t au64tckSample[SAMPLE_SIZE];
} Result;

/// Measurement runtime state settings and variables.

typedef struct {
  bool bOngoing;          ///< true: there is measurement process.going on.
  TimerId sTimer;         ///< Reference clock to read out current time value
  TimerId sAlarm;
  uint32_t u32Config;     ///< TIMG timer configuration to write into the CONFIG register in case of autoreload.
  Isr pfIsr;              ///< ISR to invoke in case of ALARM interrupt
  uint32_t u32tckPeriod;
  uint32_t u32SampleLen;  ///< Total number of samples to take.
  uint32_t u32Countdown;  ///< How many samples are still to take.
  Result *psResult;
} MeasurementState;


// ================ Local function declarations =================
static RegAddr _dport_timer_level_int_reg(ECpu eCpu, TimerId sTimer);
static void _alarm_init();
static void _alarm_isr_attach(ECpu eCpu, TimerId sTimer, uint8_t u8Int, Isr pfIsr);
static void _alarm_isr_detach(ECpu eCpu, TimerId sTimer);
static void _alarm_stop(MeasurementState *psParam, bool bReload);
static void _alarm_start(MeasurementState *psParam);

static void _uart_init();
static void _uart_cycle(uint64_t u64tckNow);
static void _alarm_reload_isr(void *pvParam);
static void _alarm_inc_isr(void *pvParam);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
static const uint32_t gu32ReloadConfig = (1 << 31) | (1 << 30) | (1 << 29) | (ALARM_DIVISOR << 13) | (1 << 11) | (1 << 10);
static const uint32_t gu32IncConfig = (1 << 31) | (1 << 30) | (ALARM_DIVISOR << 13) | (1 << 11) | (1 << 10);
static Result gsResult = {
  .u32SampleLen = 0
};
static MeasurementState gsAlarmParam = {
  .sTimer =
  {TIMG_0, TIMER0},
  .sAlarm =
  {TIMG_0, TIMER1},
  .u32Config = gu32ReloadConfig,
  .pfIsr = _alarm_reload_isr,
  .u32tckPeriod = ALARM_VAL_INIT,
  .u32SampleLen = SAMPLE_INIT,
  .u32Countdown = SAMPLE_INIT,
  .psResult = &gsResult
};

// ============== Implementation ==============
// -------------- Internal functions --------------

IRAM_ATTR static void _alarm_reload_isr(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  // clear LEVEL interrupt
  gapsTIMG[psParam->sAlarm.eTimg]->INT_CLR_TIMERS = 1 << psParam->sAlarm.eTimer;

  // storing clock value
  psParam->psResult->au64tckSample[psParam->u32SampleLen - psParam->u32Countdown] = timg_ticks(psParam->sTimer);
  --psParam->u32Countdown;

  // either allow next alarm or terminate the process
  if (0 < psParam->u32Countdown) {
    timg_tregs(psParam->sAlarm)->CONFIG = psParam->u32Config;
  } else {
    _alarm_stop(psParam, true);
  }
}

IRAM_ATTR static void _alarm_inc_isr(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  // clear LEVEL interrupt
  gapsTIMG[psParam->sAlarm.eTimg]->INT_CLR_TIMERS = 1 << psParam->sAlarm.eTimer;

  // storing clock value
  psParam->psResult->au64tckSample[psParam->u32SampleLen - psParam->u32Countdown] = timg_ticks(psParam->sTimer);
  --psParam->u32Countdown;

  // either allow next alarm or terminate the process
  if (0 < psParam->u32Countdown) {
    uint64_t u64tckAlarm = (((uint64_t)timg_tregs(psParam->sAlarm)->ALARMHI) << 32) | timg_tregs(psParam->sAlarm)->ALARMLO;
    u64tckAlarm += psParam->u32tckPeriod;
    timg_tregs(psParam->sAlarm)->ALARMLO = u64tckAlarm & 0xFFFFFFFF;
    timg_tregs(psParam->sAlarm)->ALARMHI = u64tckAlarm >> 32;
    timg_tregs(psParam->sAlarm)->CONFIG = psParam->u32Config;
  } else {
    _alarm_stop(psParam, false);
  }

}

static void _alarm_stop(MeasurementState *psParam, bool bReload) {
  timg_tregs(psParam->sAlarm)->CONFIG = 0;
  gapsTIMG[psParam->sAlarm.eTimg]->INT_ENA_TIMERS &= ~(1 << psParam->sAlarm.eTimer);
  gapsTIMG[psParam->sAlarm.eTimg]->INT_CLR_TIMERS = 1 << psParam->sAlarm.eTimer;

  psParam->psResult->bReload = bReload;
  psParam->psResult->sAlarm = psParam->sAlarm;
  psParam->psResult->u32tckPeriod = psParam->u32tckPeriod;
  psParam->psResult->u32SampleLen = psParam->u32SampleLen;

  _alarm_isr_detach(CPU_PRO, psParam->sAlarm);
  psParam->bOngoing = false;
  uart_printf(&gsUART0, " Done.\r\n");
}

static void _alarm_start(MeasurementState *psParam) {
  uart_printf(&gsUART0, "Starting measurement...");
  psParam->bOngoing = true;

  _alarm_isr_attach(CPU_PRO, psParam->sAlarm, INT_CH, psParam->pfIsr);
  timg_tregs(psParam->sAlarm)->LOADLO = 0;
  timg_tregs(psParam->sAlarm)->LOADHI = 0;
  timg_tregs(psParam->sAlarm)->LOAD = 0;
  timg_tregs(psParam->sAlarm)->ALARMLO = psParam->u32tckPeriod;
  timg_tregs(psParam->sAlarm)->ALARMHI = 0;
  gapsTIMG[psParam->sAlarm.eTimg]->INT_CLR_TIMERS = 1 << psParam->sAlarm.eTimer;
  gapsTIMG[psParam->sAlarm.eTimg]->INT_ENA_TIMERS |= 1 << psParam->sAlarm.eTimer;
  psParam->u32Countdown = psParam->u32SampleLen - 1;

  timg_tregs(psParam->sTimer)->UPDATE = 0;
  // start, inc value, autoreload, alarm enabled, generates LVL INT
  timg_tregs(psParam->sAlarm)->CONFIG = psParam->u32Config;
  psParam->psResult->au64tckSample[0] = (((uint64_t)timg_tregs(psParam->sTimer)->HI) << 32) | timg_tregs(psParam->sTimer)->LO;
}

static void _alarm_init() {
  // Nothing to do, as DPORT ENA flags are already turned on for TIMG0 and TIMG1 by default.
  // (And timer0_0 is already in use...)
}

static RegAddr _dport_timer_level_int_reg(ECpu eCpu, TimerId sTimer) {
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_TG_T0_LEVEL_INT_MAP : &dport_regs()->APP_TG_T0_LEVEL_INT_MAP);
  if (sTimer.eTimg != TIMG_0) {
    prDportIntMap += 4;
  }
  if (sTimer.eTimer != TIMER0) {
    prDportIntMap += 1;
  }
  return prDportIntMap;
}

static void _alarm_isr_detach(ECpu eCpu, TimerId sTimer) {
  RegAddr prDportIntMap = _dport_timer_level_int_reg(eCpu, sTimer);
  *prDportIntMap = 16;  // default value
}

static void _alarm_isr_attach(ECpu eCpu, TimerId sTimer, uint8_t u8Int, Isr pfCallback) {
  // register ISR and enable it
  RegAddr prDportIntMap = _dport_timer_level_int_reg(eCpu, sTimer);
  *prDportIntMap = u8Int;

  _xtos_set_interrupt_handler_arg(u8Int, pfCallback, (int)&gsAlarmParam);
  ets_isr_unmask(1 << u8Int);
}

static void _uart_init() {
  gsUART0.CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);
}

static void _uart_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = MS2TICKS(1000);
  static bool bAutoreload = true;
  static bool bPrintResult = false;
  static uint32_t u32PrintNR = 0;

  if (u64tckNext <= u64tckNow) {
    while (0 < (gsUART0.STATUS & 0xff)) {
      if (gsAlarmParam.bOngoing) break;
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      int i32AlarmValDiff = 0;
      int i32SampleLengthDiff = 0;
      switch (cCtrl) {
        case '1': // TIMG_0 TIMER1
          gsAlarmParam.sAlarm = (TimerId){TIMG_0, TIMER1};
          uart_printf(&gsUART0, "Using timer0_1\r\n");
          break;
        case '2': // TIMG_1 TIMER0
          gsAlarmParam.sAlarm = (TimerId){TIMG_1, TIMER0};
          uart_printf(&gsUART0, "Using timer1_0\r\n");
          break;
        case '3': // TIMG_1 TIMER1
          gsAlarmParam.sAlarm = (TimerId){TIMG_1, TIMER1};
          uart_printf(&gsUART0, "Using timer1_1\r\n");
          break;
        case 'a': // force autoreload
        case 'b': // force alarm inc
          bAutoreload = (cCtrl == 'a');
          gsAlarmParam.u32Config = bAutoreload ? gu32ReloadConfig : gu32IncConfig;
          gsAlarmParam.pfIsr = bAutoreload ? _alarm_reload_isr : _alarm_inc_isr;
          uart_printf(&gsUART0, "Recurrent alarm mode: %s\r\n", bAutoreload ? "autoreload" : "increment alarm value");
          break;
        case 'r': // print result
          u32PrintNR = 0;
          bPrintResult = true;
          break;
        case 's': // start measurement
          _alarm_start(&gsAlarmParam);
          break;
        case 'c': // current Reg values
          uart_printf(&gsUART0, "Alarm: {curr: %u, alarm: %u,%u, conf: %08X}\r\n",
                  (uint32_t)timg_ticks(gsAlarmParam.sAlarm),
                  timg_tregs(gsAlarmParam.sAlarm)->ALARMHI,
                  timg_tregs(gsAlarmParam.sAlarm)->ALARMLO,
                  timg_tregs(gsAlarmParam.sAlarm)->CONFIG
                  );
          break;

          // Alarm value modification
        case '>':
          i32AlarmValDiff = 10;
          break;
        case '.':
          i32AlarmValDiff = 1;
          break;
        case '<':
          i32AlarmValDiff = -10;
          break;
        case ',':
          i32AlarmValDiff = -1;
          break;

          // Samlpling length
        case '}':
          i32SampleLengthDiff = 10;
          break;
        case ']':
          i32SampleLengthDiff = 1;
          break;
        case '{':
          i32SampleLengthDiff = -10;
          break;
        case '[':
          i32SampleLengthDiff = -1;
          break;
        default:
          gsUART0.FIFO = '-';
      }
      if (i32AlarmValDiff) {
        gsAlarmParam.u32tckPeriod += i32AlarmValDiff;
        if (gsAlarmParam.u32tckPeriod < ALARM_VAL_MIN)gsAlarmParam.u32tckPeriod = ALARM_VAL_MIN;
        if (ALARM_VAL_MAX < gsAlarmParam.u32tckPeriod)gsAlarmParam.u32tckPeriod = ALARM_VAL_MAX;
        uart_printf(&gsUART0, "Alarm value: %u\r\n", gsAlarmParam.u32tckPeriod);
      }
      if (i32SampleLengthDiff) {
        gsAlarmParam.u32SampleLen += i32SampleLengthDiff;
        if (gsAlarmParam.u32SampleLen < SAMPLE_MIN)gsAlarmParam.u32SampleLen = SAMPLE_MIN;
        if (SAMPLE_MAX < gsAlarmParam.u32SampleLen)gsAlarmParam.u32SampleLen = SAMPLE_MAX;
        uart_printf(&gsUART0, "Sample length: %u\r\n", gsAlarmParam.u32SampleLen);
      }
    }
    if (bPrintResult) {
      uint32_t u32Actual = gsResult.au64tckSample[u32PrintNR] - gsResult.au64tckSample[0];
      uint32_t u32Increment = gsAlarmParam.u32tckPeriod + (gsResult.bReload ? CONST_ALARM_DELAY_TCK : 0);
      if (gsResult.bReload && (u32Increment % 2 == 0)) --u32Increment;
      uint32_t u32Expected = u32PrintNR * (u32Increment);
      int32_t i32Diff = u32Actual - u32Expected;
      uart_printf(&gsUART0, "%u\t%u\t%u\t%d\r\n",
              u32PrintNR, u32Actual, u32Expected, i32Diff);
      ++u32PrintNR;
      if (u32PrintNR == gsAlarmParam.u32SampleLen) {
        bPrintResult = false;
      }
    }
    u64tckNext += MS2TICKS(bPrintResult ? UART_FLUSH_PERIOD_MS : UART_PERIOD_MS);
  }
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  _uart_init();
  _alarm_init();
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
