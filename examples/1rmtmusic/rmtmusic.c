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
#include "utils/rmtutils.h"
#include "esp_attr.h"
#include "music.h"

// =================== Hard constants =================

// #1: Timings

#define DUTY_HI_CENT         25U    ///< ratio (percent) of HI output value in RMT CARRIER waveform.

#define UPDATE_PERIOD_MS   250U     ///< Period of the control cycle (_rmtmusic_cycle())

// #2: Channels / wires / addresses
#define RMTMUSIC_GPIO        2U
#define RMTMUSIC_CH     RMT_CH0
#define RMTINT_CH           23U     ///< Interrupt channel we allocate for RMT

// #3: Sizes
#define RMTMUSIC_MEM_BLOCKS  1U
#define RMT_TXLIM        ((RMTMUSIC_MEM_BLOCKS * RMT_RAM_BLOCK_SIZE)/2) ///< Default initial txlim value. Overridden in this example.
#define RMT_FEED0SIZE    (2 * RMT_TXLIM)  ///< Unused
#define NOTE2REG_BUFSIZE     8U           ///< A single note period (hi and lo parts) has to fit into this many registers.

typedef struct {
  int8_t *pi8Dat;
  uint8_t u8Len;
} SInt8Array;

/**
 * This structure contains all the dynamic data we need when a TXLIM interrupt happens.
 */
typedef struct {
  SMusicNote *psMusic;
  uint32_t u32MusicLen;
  uint32_t u32MusicIt;
  uint32_t u32RmtRamFillIt;
  uint32_t u32NextDuty;
  uint8_t u8RmtRamLastLoLen;
  uint8_t u8RmtRamCurHiLen;
  uint8_t u8RmtRamCurLoLen;
} SMusicRmtStateDesc;

// ================ Local function declarations =================
static uint8_t _period_to_entrypair(uint32_t *pu32Dest, uint8_t u8DestSize, uint32_t u32Period, bool bLevel);
static uint8_t _note_to_registers(uint32_t *pu32CDuty, uint32_t *pu32RamBuf, uint8_t u8BufLen, const SMusicNote *psNote, uint8_t *pu8HiLen);

static void _rmt_config_channel(ERmtChannel eChannel, bool bLevel, bool bHoldLevel);
static void _rmtmusic_init();
static void _rmtmusic_cycle(uint64_t u64Ticks);

// =================== Global constants ================
const bool gbStartAppCpu = START_APP_CPU;
const uint16_t gu16Tim00Divisor = TIM0_0_DIVISOR;
const uint64_t gu64tckSchedulePeriod = (CLK_FREQ_HZ / SCHEDULE_FREQ_HZ);

// ==================== Local Data ================
/**
 * Example music given in SMusicNote raw format.
 */
SMusicNote gasMusic[] = {
  {.acRaw = "4C.3t"},
  {.acRaw = "4A.3t"},
  {.acRaw = "2G.3t"},
  {.acRaw = "8E.3l"},
  {.acRaw = "8G.3l"},
  {.acRaw = "8F.3l"},
  {.acRaw = "8E.3l"},
  {.acRaw = "4D.3t"},
  {.acRaw = "4C.3c"},
  {.acRaw = "8C.3t"},
  {.acRaw = "8...."} // pause
};

// We generate the variations with the following pitch shifts. The base beat of the variations is LEN_64.
static int8_t gai8VarShift0[] = {36, 24, 12, 24};
static int8_t gai8VarShift1[] = {24, 19, 12, 19, 24, 31, 12, 17};
static int8_t gai8VarShift2[] = {0, 1, 0, 0, -1, 0};

static SInt8Array gasVarShift[] = {
  {gai8VarShift0, ARRAY_SIZE(gai8VarShift0)},
  {gai8VarShift1, ARRAY_SIZE(gai8VarShift1)},
  {gai8VarShift2, ARRAY_SIZE(gai8VarShift2)}
}; ///< Pitch shiftes of the different variations organized in an array.
static SMusicNote gaasMusicVariation[ARRAY_SIZE(gasVarShift)][9 * 16]; ///< The different variations will be stored here.
static uint32_t gau32VariationLen[ARRAY_SIZE(gasVarShift)]; ///< The length of the variations will be stored here.

static SMusicNote *gapsMusicRotation[ARRAY_SIZE(gasVarShift) + 1][2]; ///< There is a rotation: after one music [0] has been played, another [1] follows it. Initializer function fills this array.
static uint32_t gau32RotationLen[ARRAY_SIZE(gasVarShift) + 1]; ///< This array will contain the length of the follower musics.

static SMusicRmtStateDesc gsMusicState; ///< ISR parameter

// ==================== Implementation ================

/**
 * Transforms a single ON / OFF signal given as (u32Period, bLevel) pair
 * into RMT entries so that the result is one or more entry pairs.
 * @param pu32Dest Here to write the entry pairs.
 * @param u8DestSize Capacity of the destination array.
 * @param u32Period Length of the signal (given in RMT ticks).
 * @param bLevel Level of the signal.
 * @return Number of written items (entry pairs).
 */
static uint8_t _period_to_entrypair(uint32_t *pu32Dest, uint8_t u8DestSize, uint32_t u32Period, bool bLevel) {
  uint8_t u8Ret = 0;
  uint16_t *pu16RamBuf = (uint16_t *) pu32Dest;
  while (u32Period && u8Ret < 2 * u8DestSize) {
    uint16_t u16Slice = (RMT_ENTRYMAX < u32Period) ? RMT_ENTRYMAX : u32Period;
    pu16RamBuf[u8Ret++] = u16Slice | (bLevel ? RMT_SIGNAL1 : RMT_SIGNAL0);
    u32Period -= u16Slice;
  }
  if ((u8Ret == 2 * u8DestSize) && 0 < u32Period) { // error: buffer is too small
    return -1;
  }
  if (0 < (u8Ret & 1)) { // odd number of entries filled
    pu16RamBuf[u8Ret++] = pu16RamBuf[0] - 1;
    pu16RamBuf[0] = 1 | (bLevel ? RMT_SIGNAL1 : RMT_SIGNAL0);
  }
  if (!pu16RamBuf[1]) { // exceptional case: u32Period was originally 1. Skip it!
    return 0; // and sorry for writing into prRamBuf
  }
  return u8Ret / 2;
}

/**
 * This routine transforms a single SMusicNote into the RMT registers (carrier duty and ram buffer registers).
 * @param pu32CDuty Here to write the carrier duty register value.
 * @param pu32RamBuf Here to write the RAM buffer registers.
 * @param u8BufLen Size (capacity) of pu32RamBuf array.
 * @param psNote Musical note to transform.
 * @param pu8HiLen Here to write the number of items where the HI level of the note were written.
 * @return Total number of written items.
 */
static uint8_t _note_to_registers(uint32_t *pu32CDuty, uint32_t *pu32RamBuf, uint8_t u8BufLen, const SMusicNote *psNote, uint8_t *pu8HiLen) {
  uint8_t u8Ret = 0;

  // raw values
  NoteIdx u8NoteIdx = note_get_idx(psNote);
  uint32_t u32TonePeriodLen = noteidx_to_wperiod(u8NoteIdx);
  uint32_t u32NoteLen = duration_ticks(psNote->eLen);
  uint32_t u32NoteHiLen = note_is_pause(psNote) ? 0U : notelen_fill(u32NoteLen, psNote->eFill);
  uint32_t u32NoteLoLen = u32NoteLen - u32NoteHiLen;

  if (!u32TonePeriodLen) {
    u32TonePeriodLen = 1 << 16;
  }
  uint16_t u16High = (u32TonePeriodLen * DUTY_HI_CENT) / 100;
  uint16_t u16Low = u32TonePeriodLen - u16High;
  *pu32CDuty = u16High << 16 | u16Low;

  // there is two phases: active (RMT output: high) and inactive (RMT output: low)
  u8Ret += _period_to_entrypair(&pu32RamBuf[u8Ret], u8BufLen - u8Ret, u32NoteHiLen, true);
  if (pu8HiLen) {
    *pu8HiLen = u8Ret;
  }
  u8Ret += _period_to_entrypair(&pu32RamBuf[u8Ret], u8BufLen - u8Ret, u32NoteLoLen, false);

  return u8Ret;
}

static void _rmt_config_channel(ERmtChannel eChannel, bool bLevel, bool bHoldLevel) {
  // rmt channel config
  // rmt channel config
  SRmtChConf rChConf = {
    .r0 =
    {.u8DivCnt = RMT_DIVISOR, .u4MemSize = RMTMUSIC_MEM_BLOCKS, .bCarrierEn = 1, .bCarrierOutLvl = 1},
    .r1 =
    {.bRefAlwaysOn = 0, .bRefCntRst = 1, .bMemRdRst = 1, .bIdleOutLvl = bLevel, .bIdleOutEn = bHoldLevel}
  };
  gpsRMT->asChConf[eChannel] = rChConf;

  {
    SRmtChCarrierDutyReg rChCarr = {.u16High = 1000, .u16Low = 1000};
    gpsRMT->arCarrierDuty[eChannel] = rChCarr;
  }

  // set memory ownership of RMT RAM blocks
  SRmtChConf1Reg sRdMemCfg = {.raw = -1};
  sRdMemCfg.bMemOwner = 0;
  for (int i = 0; i < RMTMUSIC_MEM_BLOCKS; ++i) {

    gpsRMT->asChConf[(eChannel + i) % RMT_CHANNEL_NUM].r1.raw &= sRdMemCfg.raw;
  }

  gpsRMT->arTxLim[eChannel].u9Val = RMT_TXLIM; // half of the memory block
}

IRAM_ATTR void _rmtmusic_feed(void *pvParam) {
  SMusicRmtStateDesc *psParam = (SMusicRmtStateDesc*) pvParam;

  gpsRMT->arCarrierDuty[RMTMUSIC_CH].raw = psParam->u32NextDuty;
  //uint8_t u8TxLim = psParam->u8RmtRamLastLoLen + psParam->u8RmtRamCurHiLen;
  uint8_t u8TxLim = psParam->u8RmtRamCurLoLen + psParam->u8RmtRamCurHiLen;
  gpsRMT->arTxLim[RMTMUSIC_CH].u9Val = u8TxLim;

  psParam->u8RmtRamLastLoLen = psParam->u8RmtRamCurLoLen;

  uint8_t u8HiLen;
  uint32_t au32Buf[NOTE2REG_BUFSIZE];
  uint8_t u8BufLen = _note_to_registers(&psParam->u32NextDuty, au32Buf, NOTE2REG_BUFSIZE, &psParam->psMusic[psParam->u32MusicIt++], &u8HiLen);
  psParam->u32RmtRamFillIt = rmtutils_copytoram(RMTMUSIC_CH, RMTMUSIC_MEM_BLOCKS, psParam->u32RmtRamFillIt, au32Buf, u8BufLen);
  psParam->u32RmtRamFillIt %= RMTMUSIC_MEM_BLOCKS * RMT_RAM_BLOCK_SIZE;
  psParam->u8RmtRamCurHiLen = u8HiLen;
  psParam->u8RmtRamCurLoLen = u8BufLen - u8HiLen;

  if (psParam->u32MusicIt == psParam->u32MusicLen) {
    // rotate music
    bool bFound = false;
    for (int i = 0; i < ARRAY_SIZE(gapsMusicRotation) && !bFound; ++i) {
      if (psParam->psMusic == gapsMusicRotation[i][0]) {
        bFound = true;
        psParam->psMusic = gapsMusicRotation[i][1];
        psParam->u32MusicLen = gau32RotationLen[i];
      }
    }
    psParam->u32MusicIt = 0;
  }
}

static void _rmtmusic_init() {

  // create variations and update the rotation array
  for (int i = 0; i < ARRAY_SIZE(gasVarShift); ++i) {
    gau32VariationLen[i] = music_create_variation(
            gaasMusicVariation[i], ARRAY_SIZE(gaasMusicVariation[i]),
            gasMusic, ARRAY_SIZE(gasMusic),
            gasVarShift[i].pi8Dat, gasVarShift[i].u8Len);

    gapsMusicRotation[i][1] = gaasMusicVariation[i];
    gapsMusicRotation[i + 1][0] = gaasMusicVariation[i];
    gau32RotationLen[i] = gau32VariationLen[i];
  }
  gapsMusicRotation[ARRAY_SIZE(gasVarShift)][1] = gasMusic;
  gapsMusicRotation[0][0] = gasMusic;
  gau32RotationLen[ARRAY_SIZE(gasVarShift)] = ARRAY_SIZE(gasMusic);

  // initialize MCU parts
  rmt_init_controller(true, true);
  rmt_init_channel(RMTMUSIC_CH, RMTMUSIC_GPIO, false);
  _rmt_config_channel(RMTMUSIC_CH, 0, 0);

  // we do some logging, hence set UART0 speed
  gsUART0.CLKDIV = APB_FREQ_HZ / 115200;

  // register ISR and enable it
  rmt_isr_init();
  rmt_isr_register(RMTMUSIC_CH, NULL, NULL, _rmtmusic_feed, NULL, &gsMusicState);
  rmt_isr_start(CPU_PRO, RMTINT_CH);
}

static void _rmtmusic_cycle(uint64_t u64Ticks) {
  static uint64_t u64NextTick = MS2TICKS(UPDATE_PERIOD_MS);
  static bool bFirstRun = true;

  if (u64NextTick <= u64Ticks) {
    if (bFirstRun) {
      gsMusicState = (SMusicRmtStateDesc){
        .psMusic = gasMusic,
        .u32MusicLen = ARRAY_SIZE(gasMusic),
        .u32MusicIt = 0,
        .u32RmtRamFillIt = 0,
        .u8RmtRamLastLoLen = 0,
        .u8RmtRamCurHiLen = 0,
        .u8RmtRamCurLoLen = 0
      };
      uint32_t au32Buf[NOTE2REG_BUFSIZE];
      uint8_t u8HiLen0;
      uint8_t u8BufLen0 = _note_to_registers((uint32_t*)&gsMusicState.u32NextDuty, au32Buf, NOTE2REG_BUFSIZE, &gsMusicState.psMusic[gsMusicState.u32MusicIt++], &u8HiLen0);
      gsMusicState.u8RmtRamLastLoLen = u8BufLen0 - u8HiLen0;
      gsMusicState.u32RmtRamFillIt = rmtutils_copytoram(RMTMUSIC_CH, RMTMUSIC_MEM_BLOCKS, 0, au32Buf, u8BufLen0);
      gpsRMT->arCarrierDuty[RMTMUSIC_CH].raw = gsMusicState.u32NextDuty;
      gpsRMT->arTxLim[RMTMUSIC_CH].u9Val = u8HiLen0 + 2;

      uint8_t u8HiLen1;
      uint8_t u8BufLen1 = _note_to_registers((uint32_t*)&gsMusicState.u32NextDuty, au32Buf, NOTE2REG_BUFSIZE, &gsMusicState.psMusic[gsMusicState.u32MusicIt++], &u8HiLen1);
      gsMusicState.u32RmtRamFillIt = rmtutils_copytoram(RMTMUSIC_CH, RMTMUSIC_MEM_BLOCKS, gsMusicState.u32RmtRamFillIt, au32Buf, u8BufLen1);
      gsMusicState.u8RmtRamCurHiLen = u8HiLen1;
      gsMusicState.u8RmtRamCurLoLen = u8BufLen1 - u8HiLen1;

      rmt_start_tx(RMTMUSIC_CH, true);
      bFirstRun = false;
    }
    u64NextTick += MS2TICKS(UPDATE_PERIOD_MS);
  }
}

// ====================== Interface functions =========================

void prog_init_pro_pre() {

  _rmtmusic_init();
}

void prog_init_app() {
}

void prog_init_pro_post() {
}

void prog_cycle_app(uint64_t u64tckNow) {
}

void prog_cycle_pro(uint64_t u64tckNow) {
  _rmtmusic_cycle(u64tckNow);
}
