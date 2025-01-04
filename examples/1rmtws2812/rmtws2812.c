/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "dport.h"
#include "gpio.h"
#include "main.h"
#include "rmt.h"
#include "defines.h"
#include "romfunctions.h"
#include "print.h"
#include "uart.h"
#include "iomux.h"
#include "dport.h"
#include "typeaux.h"
#include "utils/generators.h"
#include "utils/rmtutils.h"
#include "esp_attr.h"

// =================== Hard constants =================

// #1: Timings -- common TICK base: 50ns: 20MHz
#define WS2812_0H_NS       400U
#define WS2812_0L_NS       850U
#define WS2812_1H_NS       800U
#define WS2812_1L_NS       450U
#define WS2812_RES_US       50U // Reset length given in µs

#define UPDATE_PERIOD_MS      50U   ///< WS2812B update period
#define BUF_UPDATE_PERIOD_MS  50U   ///< Data update period
#define RMT_DIVISOR            4U   ///< RMT TICK freq is 80MHz / RMT_DIVISOR (4 -> 20MHz, 8 -> 10MHz, 2 -> 40MHz)

// RMT carrier settings
#define CARRIER_EN           0U     ///< no carrier
#define CARRIER_HI_TCK   40000U
#define CARRIER_LO_TCK   40000U

#define RMTCLK_FREQ_HZ         (APB_FREQ_HZ / RMT_DIVISOR)        // 20 MHz
#define RMTTICKS_PER_MS        (RMTCLK_FREQ_HZ / 1000U)           // 20000
#define RMTTICKS_PER_US        (RMTCLK_FREQ_HZ / 1000000U)        // 20
#define RMTNS_PER_TICKS        (1000 / RMTTICKS_PER_US)           // 50

// #2: Channels / wires / addresses
#define RMTWS2812_GPIO      21U
#define RMTWS2812_CH    RMT_CH0
#define RMTINT_CH           23U

// #3: Sizes and iteration cycles
#define STRIP_LENGTH          12U
#define STRIP_FRONT_LEN        2U
#define STRIP_BACK_LEN         2U
#define STRIP_INTERPOLATION_STEPS   5U
#define STRIP_GRADCHANGE_STEPS     30U
#define STRIP_ROTATE_CYCLES         3U

// #4: RMT buffer settings
#define RMTWS2812_MEM_BLOCKS  1U
#define RMT_TXLIM        ((RMTWS2812_MEM_BLOCKS * RMT_RAM_BLOCK_SIZE) / 2)
#define RMT_FEED0SIZE    (2 * RMT_TXLIM)

// ============= Local types ===============

typedef struct {
  SBitPwmGenState sGen;
  uint16_t u16TxPos;
} SBitFeederState;

typedef uint8_t Color[3];

// ================ Local function declarations =================

static void _rmt_init_controller();
static void _rmt_init_channel(ERmtChannel eChannel, uint8_t u8Pin, bool bLevel, bool bHoldLevel);
static void _fill_prebuffer(uint8_t *pu8Dest, uint8_t u8Stop0Idx, uint8_t u8Stop1Idx);
static uint8_t _weighted_avg(uint8_t u8A, uint8_t u8B, uint32_t u32WeightA, uint32_t u32WeightB);
static uint8_t *_buf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint8_t *pu8B, uint32_t u32WeightA, uint32_t u32WeightB);
static uint8_t *_rotbuf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint32_t u32AOffset, uint8_t *pu8B, uint32_t u32BOffset, uint32_t u32WeightA, uint32_t u32WeightB);
void _rmtws2812_feed(void *pvParam);
static void _rmtws2812_init();
static void _rmtws2812_cycle(uint64_t u64Ticks);
static void _buf_update_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);
const uint16_t gau16tckPhaseLen[] = {
  WS2812_0H_NS / RMTNS_PER_TICKS,
  WS2812_0L_NS / RMTNS_PER_TICKS,
  WS2812_1H_NS / RMTNS_PER_TICKS,
  WS2812_1L_NS / RMTNS_PER_TICKS,
  (1000 * WS2812_RES_US) / RMTNS_PER_TICKS
};

const Color gasStops[] = {
  {0x00, 0xFF, 0x00},   // R
  {0x5F, 0xBF, 0x00},   // Y
  {0x00, 0x00, 0xFF},   // B
  {0x7F, 0x00, 0x00},   // G
  {0x00, 0x7F, 0xBF},   // M
  {0x00, 0x00, 0x00},   // Off
  {0x5F, 0x00, 0xBF},   // C
  {0x5F, 0x7F, 0xBF}    // W
};

// ==================== Local Data ================

static uint8_t gau8PreBuffer0[3 * STRIP_LENGTH];
static uint8_t gau8PreBuffer1[3 * STRIP_LENGTH];
static uint8_t gau8Buffer[3 * STRIP_LENGTH];
static SBitFeederState gsBitFeederState;

// ==================== Implementation ================

static void _rmt_init_controller() {
  dport_regs()->PERIP_CLK_EN |= 1 << DPORT_PERIP_BIT_RMT;

  dport_regs()->PERIP_RST_EN |= 1 << DPORT_PERIP_BIT_RMT;
  dport_regs()->PERIP_RST_EN &= ~(1 << DPORT_PERIP_BIT_RMT);

  SRmtApbConfReg rApbConf = {.bMemAccessEn = 1, .bMemTxWrapEn = 1}; // direct RMT RAM access (not using FIFO), mem wrap-around
  gpsRMT->rApb = rApbConf;
}

static void _rmt_init_channel(ERmtChannel eChannel, uint8_t u8Pin, bool bLevel, bool bHoldLevel) {
  // gpio & iomux
  bLevel ? gpio_pin_out_on(u8Pin) : gpio_pin_out_off(u8Pin); // set GPIO level when not bound to RMT (optional)

  IomuxGpioConfReg rRmtConf = {.u1FunIE = 1, .u1FunWPU = 1, .u3McuSel = 2}; // input enable, pull-up, iomux function
  iomux_set_gpioconf(u8Pin, rRmtConf);

  gpio_pin_enable(u8Pin);
  gpio_matrix_out(u8Pin, rmt_out_signal(eChannel), 0, 0);
  gpio_matrix_in(u8Pin, rmt_in_signal(eChannel), 0);

  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = RMT_DIVISOR, .u4MemSize = RMTWS2812_MEM_BLOCKS, .bCarrierEn = CARRIER_EN, .bCarrierOutLvl = 1},
    .r1 =
    {.bRefAlwaysOn = 1, .bRefCntRst = 1, .bMemRdRst = 1, .bIdleOutLvl = bLevel, .bIdleOutEn = bHoldLevel}
  };
  gpsRMT->asChConf[eChannel] = rChConf;

  if (CARRIER_EN) {
    SRmtChCarrierDutyReg rChCarr = {.u16High = CARRIER_HI_TCK, .u16Low = CARRIER_LO_TCK};
    gpsRMT->arCarrierDuty[eChannel] = rChCarr;
  }

  // set memory ownership of RMT RAM blocks
  SRmtChConf1Reg sRdMemCfg = {.raw = -1};
  sRdMemCfg.bMemOwner = 0;
  for (int i = 0; i < RMTWS2812_MEM_BLOCKS; ++i) {
    gpsRMT->asChConf[(eChannel + i) % RMT_CHANNEL_NUM].r1.raw &= sRdMemCfg.raw;
  }

  gpsRMT->arTxLim[eChannel].u9Val = RMT_TXLIM; // half of the memory block
  gpsRMT->arInt[RMT_INT_ENA] =
          rmt_int_bit(eChannel, RMT_INT_TXEND) |
          rmt_int_bit(eChannel, RMT_INT_TXTHRES) |
          rmt_int_bit(eChannel, RMT_INT_ERR);
}

static void _fill_prebuffer(uint8_t *pu8Dest, uint8_t u8Stop0Idx, uint8_t u8Stop1Idx) {
  for (int i = 0; i < STRIP_FRONT_LEN; ++i) {
    memcpy(pu8Dest + (3 * i), gasStops[u8Stop0Idx], 3);
  }
  for (int i = STRIP_FRONT_LEN; i < (STRIP_LENGTH - STRIP_BACK_LEN); ++i) {
    int div = STRIP_LENGTH - STRIP_FRONT_LEN - STRIP_BACK_LEN + 1;
    int j = i - STRIP_FRONT_LEN + 1;  // jmax=(imax=STRIP_LENGTH - STRIP_BACK_LEN - 1) - STRIP_FRONT_LEN + 1 = div - 1
    int k = div - j;
    pu8Dest[3 * i] = (k * gasStops[u8Stop0Idx][0] + j * gasStops[u8Stop1Idx][0]) / div;
    pu8Dest[3 * i + 1] = (k * gasStops[u8Stop0Idx][1] + j * gasStops[u8Stop1Idx][1]) / div;
    pu8Dest[3 * i + 2] = (k * gasStops[u8Stop0Idx][2] + j * gasStops[u8Stop1Idx][2]) / div;
  }
  for (int i = STRIP_LENGTH - STRIP_BACK_LEN; i < STRIP_LENGTH; ++i) {
    memcpy(pu8Dest + (3 * i), gasStops[u8Stop1Idx], 3);
  }
}

static uint8_t _weighted_avg(uint8_t u8A, uint8_t u8B, uint32_t u32WeightA, uint32_t u32WeightB) {
  return (u32WeightA * u8A + u32WeightB * u8B) / (u32WeightA + u32WeightB);
}

static uint8_t *_buf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint8_t *pu8B, uint32_t u32WeightA, uint32_t u32WeightB) {
  for (uint32_t i = 0; i < u32Len; ++i) {
    pu8Res[i] = _weighted_avg(pu8A[i], pu8B[i], u32WeightA, u32WeightB);
  }
  return pu8Res;
}

static uint8_t *_rotbuf_weighted_avg(uint8_t *pu8Res, uint32_t u32Len, uint8_t *pu8A, uint32_t u32AOffset, uint8_t *pu8B, uint32_t u32BOffset, uint32_t u32WeightA, uint32_t u32WeightB) {
  for (uint32_t i = 0; i < u32Len; ++i) {
    pu8Res[i] = _weighted_avg(pu8A[(u32AOffset + i) % u32Len], pu8B[(u32BOffset + i) % u32Len], u32WeightA, u32WeightB);
  }
  return pu8Res;
}

IRAM_ATTR void _rmtws2812_feed(void *pvParam) {
  SBitFeederState *psParam = (SBitFeederState*)pvParam;

  if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_TXTHRES)) {
    gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_TXTHRES);
    rmtutils_feed_tx(RMTWS2812_CH, &psParam->u16TxPos, RMT_TXLIM, (U16Generator)bitpwmgen_next, (UniRel)bitpwmgen_end, (void*)&psParam->sGen);
  }

  if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR)) {
    gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR);
    gsUART0.FIFO = 'r';
  }

  if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND)) {
    gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND);
  }
}

static void _rmtws2812_init() {
  SBitGenState sBitGen = bitgen_init(0, false, WS2812_1H_NS / RMTNS_PER_TICKS, WS2812_0H_NS / RMTNS_PER_TICKS);
  sBitGen.u8BitIdx = 8;
  SByteGenState sByteGen = bytegen_init(gau8Buffer, ARRAY_SIZE(gau8Buffer));
  SPwmXGenState sPwmXGen = {
    .u8CurValue = 0,
    .u8HiUpper = 0x80,
    .u8LoUpper = 0x00,
    .u8PeriodLen = (WS2812_1H_NS + WS2812_1L_NS) / RMTNS_PER_TICKS,
    .u8PhaseIdx = 2
  };
  gsBitFeederState.sGen = (SBitPwmGenState){.sBitGenState = sBitGen, .sByteGenState = sByteGen, .sPwmXGenState = sPwmXGen};
  gsBitFeederState.u16TxPos = 0;

  _fill_prebuffer(gau8PreBuffer0, 0, 1);
  _buf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), gau8PreBuffer0, gau8PreBuffer0, 1, 0);

  _rmt_init_controller();
  _rmt_init_channel(RMTWS2812_CH, RMTWS2812_GPIO, 0, 0);

  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV = APB_FREQ_HZ / 115200;

  // register ISR and enable it
  ECpu eCpu = CPU_PRO;
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_RMT_INTR_MAP : &dport_regs()->APP_RMT_INTR_MAP);

  *prDportIntMap = RMTINT_CH;
  _xtos_set_interrupt_handler_arg(RMTINT_CH, _rmtws2812_feed, (int) &gsBitFeederState);
  ets_isr_unmask(1 << RMTINT_CH);
}

static void _rmtws2812_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = 0;

  static bool bFirstRun = true;

  if (bFirstRun) {

    rmtutils_feed_tx(RMTWS2812_CH, &gsBitFeederState.u16TxPos, RMT_FEED0SIZE, (U16Generator)bitpwmgen_next, (UniRel)bitpwmgen_end, (void*)&gsBitFeederState.sGen);
    rmt_start_tx(RMTWS2812_CH, true);

    bFirstRun = false;
  }

  if (u64NextTick <= u64Ticks) {
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_TXEND);
      gsUART0.FIFO = 'E';
    }
    if (bitpwmgen_end(&gsBitFeederState.sGen)) {
      bitpwmgen_reset(&gsBitFeederState.sGen);
      gsBitFeederState.u16TxPos = 0U;

      rmtutils_feed_tx(RMTWS2812_CH, &gsBitFeederState.u16TxPos, RMT_FEED0SIZE, (U16Generator)bitpwmgen_next, (UniRel)bitpwmgen_end, (void*)&gsBitFeederState.sGen);
      rmt_start_tx(RMTWS2812_CH, true);
    }
    if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR)) {
      gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(RMTWS2812_CH, RMT_INT_ERR);
      gsUART0.FIFO = 'R';
    }
    u64NextTick += MS2TICKS(UPDATE_PERIOD_MS);
  }
}

static void _buf_update_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(1025);
  static uint32_t u32Shift = 0;
  static uint32_t u32SubShift = 0;
  static uint8_t u8RotCycleCnt = 0;
  static bool bRotateBuffer = true; // true: cycle within buffer with shift and subshift, false: replace gradient
  static bool bUsePreBuf0 = true;   // which buffer to use (PreBuf0 vs. PreBuf1)
  static uint8_t u8Stop0Idx = 0;    // Color index on gasStops, identifies the color at the buffer begin
  static uint8_t u8Stop1Idx = 1;    // Color index on gasStops, identifies the color at the buffer end
  static bool bStopSwap = false;

  if (u64NextTick <= u64Ticks) {
    uint8_t *pu8PreBuf = bUsePreBuf0 ? gau8PreBuffer0 : gau8PreBuffer1;   // current main prebuffer
    uint8_t *pu8XPreBuf = !bUsePreBuf0 ? gau8PreBuffer0 : gau8PreBuffer1; // other prebuffer
    if (bRotateBuffer) {
      ++u32SubShift;
      if (u32SubShift == STRIP_INTERPOLATION_STEPS) {
        u32SubShift = 0;
        ++u32Shift;
        if (u32Shift == STRIP_LENGTH) {
          u32Shift = 0;
        }
      }
      _rotbuf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), pu8PreBuf, 3 * u32Shift, pu8PreBuf, 3 * (u32Shift + 1), STRIP_INTERPOLATION_STEPS - u32SubShift, u32SubShift);
      if (u32Shift == 0 && u32SubShift == 0) {  // end of buffer rotation
        ++u8RotCycleCnt;
        if (u8RotCycleCnt == STRIP_ROTATE_CYCLES) {
          u8RotCycleCnt = 0;
          bStopSwap = !bStopSwap;
          uint8_t tmp = u8Stop1Idx;
          u8Stop1Idx = u8Stop0Idx;
          u8Stop0Idx = tmp;
          if (!bStopSwap) {
            ++u8Stop0Idx;
            ++u8Stop1Idx;
            if (u8Stop0Idx == ARRAY_SIZE(gasStops)) {
              u8Stop0Idx = 0;
              ++u8Stop1Idx;
            }
            u8Stop1Idx %= ARRAY_SIZE(gasStops);
            if (u8Stop1Idx == u8Stop0Idx) {
              ++u8Stop1Idx;
            }
          }
          _fill_prebuffer(pu8XPreBuf, u8Stop0Idx, u8Stop1Idx);
          bRotateBuffer = false;
        }
      }
    } else {  // not rotating buffer
      ++u32SubShift;
      _buf_weighted_avg(gau8Buffer, ARRAY_SIZE(gau8Buffer), pu8PreBuf, pu8XPreBuf, STRIP_GRADCHANGE_STEPS - u32SubShift, u32SubShift);
      if (u32SubShift == STRIP_GRADCHANGE_STEPS) {  // end of gradient replacement
        u32SubShift = 0;
        bRotateBuffer = true;
        bUsePreBuf0 = !bUsePreBuf0;
      }
    }
    u64NextTick += MS2TICKS(BUF_UPDATE_PERIOD_MS);
  }
}


// ====================== Interface functions =========================

void prog_init_pro_pre() {
  _rmtws2812_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtws2812_cycle(u64tckNow);
  _buf_update_cycle(u64tckNow);
}
