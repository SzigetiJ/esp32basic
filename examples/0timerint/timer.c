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

// #1: limits
#define COMPAREINC_VAL_MIN         20U
#define COMPAREINC_VAL_MAX      10000U
#define COMPAREINC_VAL_INIT       200U
#define SAMPLE_MIN            10U
#define SAMPLE_MAX           200U
#define SAMPLE_INIT           50U

// #3: Sizes
#define SAMPLE_SIZE   SAMPLE_MAX

// ============= Local types ===============

typedef enum {
  CTIMER0 = 0,
  CTIMER1 = 1,
  CTIMER2 = 2
} ECCompareIdx;

typedef struct {
  uint32_t u32cycPeriod;
  uint32_t u32SampleLen;
  uint64_t au64tckSample[SAMPLE_SIZE];
} Result;                 ///< The results of the last measurement are stored in such structure.

typedef struct {
  bool bOngoing;          ///< true: there is measurement process.going on.
  bool bGeneral;          ///< use gerenal ISR
  ECCompareIdx eCcompare;
  TimerId sTimer;         ///< Reference clock to read out current time value
  uint32_t u32cycPeriod;
  uint32_t u32SampleLen;  ///< Total number of samples to take.
  uint32_t u32SampleIdx;  ///< How many samples are still to take.
  Result *psResult;
} MeasurementState;       ///< Measurement runtime state settings and variables.


// ================ Local function declarations =================
static void _timerx_isr_attach();
static void _timerx_stop();
static void _timerx_general_isr(void *pvParam);
static void _timer0_isr(void *pvParam);
static void _timer1_isr(void *pvParam);

static void _measurement_start(MeasurementState * psParam);
static void _print_resultline(uint32_t u32Idx, const Result *psResult);

static void _uart_init();
static void _uart_cycle(uint64_t u64tckNow);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
const uint8_t gau8TimerIntNum[] = {6, 15, 16};
//const uint8_t gau8TimerIntLvl[] = {1,3,5};

DRAM_ATTR static Result gsResult = {
  .u32SampleLen = 0
};
static bool gbAppCpuStarted = false;
DRAM_ATTR static MeasurementState gsMeasParam = {
  .bOngoing = false,
  .bGeneral = false,
  .eCcompare = CTIMER0,
  .sTimer =
  {TIMG_0, CTIMER0},
  .u32cycPeriod = COMPAREINC_VAL_INIT,
  .u32SampleLen = SAMPLE_INIT,
  .u32SampleIdx = SAMPLE_INIT,
  .psResult = &gsResult
};

// ============== Implementation ==============
// -------------- Internal functions --------------

IRAM_ATTR static void _timerx_general_isr(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  uint32_t u32CurrCnt;
  // note, the xthal_xxx() register access is slower than RSR/WSR access, nevertheless, it is more elastic.
  // beeing #define macros, in case of RSR/WSR we have to explicitly set CCOMPAREn as first parameter.
  u32CurrCnt = xthal_get_ccompare(psParam->eCcompare);
  u32CurrCnt += psParam->u32cycPeriod;
  xthal_set_ccompare(psParam->eCcompare, u32CurrCnt);

  // storing clock value
  gsResult.au64tckSample[psParam->u32SampleIdx] = timg_ticks(psParam->sTimer);
  ++psParam->u32SampleIdx;

  if (psParam->u32SampleIdx == psParam->u32SampleLen) {
    _timerx_stop(pvParam);
  }
}

IRAM_ATTR static void _timer0_isr(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  uint32_t u32CurrCnt;
  RSR(CCOMPARE0, u32CurrCnt);
  u32CurrCnt += psParam->u32cycPeriod;
  WSR(CCOMPARE0, u32CurrCnt);

  // storing clock value
  gsResult.au64tckSample[psParam->u32SampleIdx] = timg_ticks(psParam->sTimer);
  ++psParam->u32SampleIdx;

  if (psParam->u32SampleIdx == psParam->u32SampleLen) {
    _timerx_stop(pvParam);
  }
}

IRAM_ATTR static void _timer1_isr(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  uint32_t u32CurrCnt;
  RSR(CCOMPARE1, u32CurrCnt);
  u32CurrCnt += psParam->u32cycPeriod;
  WSR(CCOMPARE1, u32CurrCnt);

  // storing clock value
  gsResult.au64tckSample[psParam->u32SampleIdx] = timg_ticks(psParam->sTimer);
  ++psParam->u32SampleIdx;

  if (psParam->u32SampleIdx == psParam->u32SampleLen) {
    _timerx_stop(pvParam);
  }
}

IRAM_ATTR static void _timerx_stop(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;

  ets_isr_mask(1 << gau8TimerIntNum[psParam->eCcompare]);
  _xtos_set_interrupt_handler(gau8TimerIntNum[psParam->eCcompare], NULL);

  psParam->bOngoing = false;
  uart_printf(&gsUART0, " Done.\r\n");
}

IRAM_ATTR static void _timerx_isr_attach(void *pvParam) {
  MeasurementState *psParam = (MeasurementState*)pvParam;
  Isr fIsr = psParam->bGeneral ? _timerx_general_isr :
          psParam->eCcompare == CTIMER0 ? _timer0_isr :
          psParam->eCcompare == CTIMER1 ? _timer1_isr :
          _timerx_general_isr;
  uint32_t u32Ccount;
  u32Ccount = xthal_get_ccount();
  uint32_t u32Ccompare = u32Ccount;
  u32Ccompare += psParam->u32cycPeriod;
  xthal_set_ccompare(psParam->eCcompare, u32Ccompare);

  _xtos_set_interrupt_handler_arg(gau8TimerIntNum[psParam->eCcompare], fIsr, (int)pvParam);
  ets_isr_unmask(1 << gau8TimerIntNum[psParam->eCcompare]);
}

static void _measurement_start(MeasurementState * psParam) {
  uart_printf(&gsUART0, "Starting measurement...");
  psParam->bOngoing = true;
  psParam->u32SampleIdx = 1;
  psParam->psResult->u32cycPeriod = psParam->u32cycPeriod;
  psParam->psResult->u32SampleLen = psParam->u32SampleLen;

  _timerx_isr_attach(psParam);

  psParam->psResult->au64tckSample[0] = timg_ticks(psParam->sTimer);
}

static void _uart_init() {
  gsUART0.CLKDIV.raw = UART_HZ2CLKDIV(UART_FREQ_HZ, APB_FREQ_HZ);
}

static void _uart_cycle(uint64_t u64tckNow) {
  static uint64_t u64tckNext = MS2TICKS(1000);
  static bool bPrintResult = false;
  static uint32_t u32PrintNR = 0;

  if (u64tckNext <= u64tckNow) {
    while (0 < (gsUART0.STATUS & 0xff)) {
      if (gsMeasParam.bOngoing) break;
      char cCtrl = gsUART0Mapped.FIFO & 0xff;
      int i32AlarmValDiff = 0;
      int i32SampleLengthDiff = 0;
      switch (cCtrl) {
        case '0':
        case '1':
        case '2': // TIMG_1 TIMER0
          uart_printf(&gsUART0, "Using ccompare%c\r\n", cCtrl);
          gsMeasParam.eCcompare = cCtrl - '0';
          break;
        case 'r': // print result
          u32PrintNR = 0;
          bPrintResult = true;
          break;
        case 's': // start measurement
          _measurement_start(&gsMeasParam);
          break;
        case 'g':
          gsMeasParam.bGeneral = true;
          uart_printf(&gsUART0, "Using general ISR\r\n");
          break;
        case 'G':
          gsMeasParam.bGeneral = false;
          uart_printf(&gsUART0, "Using CCOMPAREn-specific ISR\r\n");
          break;
        case 'c': // current Reg values
          break;
        case 'C':
          uart_printf(&gsUART0, "ISR addr: %p, dat: %p %p, size: %u\r\n", _timerx_general_isr, &gsResult, &gsMeasParam, sizeof (MeasurementState));
          break;
        case 'i':
          uart_printf(&gsUART0, "Ccompare%u\r\n", gsMeasParam.eCcompare);
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
        gsMeasParam.u32cycPeriod += i32AlarmValDiff;
        if (gsMeasParam.u32cycPeriod < COMPAREINC_VAL_MIN) gsMeasParam.u32cycPeriod = COMPAREINC_VAL_MIN;
        if (COMPAREINC_VAL_MAX < gsMeasParam.u32cycPeriod) gsMeasParam.u32cycPeriod = COMPAREINC_VAL_MAX;
        uart_printf(&gsUART0, "Ccompare increment: %u\r\n", gsMeasParam.u32cycPeriod);
      }
      if (i32SampleLengthDiff) {
        gsMeasParam.u32SampleLen += i32SampleLengthDiff;
        if (gsMeasParam.u32SampleLen < SAMPLE_MIN)gsMeasParam.u32SampleLen = SAMPLE_MIN;
        if (SAMPLE_MAX < gsMeasParam.u32SampleLen)gsMeasParam.u32SampleLen = SAMPLE_MAX;
        uart_printf(&gsUART0, "Sample length: %u\r\n", gsMeasParam.u32SampleLen);
      }
    }
    if (bPrintResult) {
      _print_resultline(u32PrintNR, &gsResult);
      ++u32PrintNR;
      if (u32PrintNR == gsResult.u32SampleLen) {
        bPrintResult = false;
      }
    }

    u64tckNext += MS2TICKS(bPrintResult ? UART_FLUSH_PERIOD_MS : UART_PERIOD_MS);
  }
}

static void _print_resultline(uint32_t u32Idx, const Result * psResult) {
  uint32_t u32Actual = psResult->au64tckSample[u32Idx] - psResult->au64tckSample[0];
  uint32_t u32Increment = psResult->u32cycPeriod;
  uint32_t u32Expected = (u32Idx * (u32Increment)) / (2 * TIM0_0_DIVISOR);
  int32_t i32Diff = u32Actual - u32Expected;
  if (u32Idx == 0) {  // print header
    uart_printf(&gsUART0, "idx\tact\texp\tdiff\traw\r\n");
  }
  uart_printf(&gsUART0, "%u\t%u\t%u\t%d\t%u\r\n",
          u32Idx, u32Actual, u32Expected, i32Diff, psResult->au64tckSample[u32Idx]);
}

// -------------- Interface functions --------------

void prog_init_pro_pre() {
  _uart_init();
}

void prog_init_app() {
  gbAppCpuStarted = true;
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _uart_cycle(u64tckNow);
}
