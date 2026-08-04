/*
 * Copyright 2026 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "efuse.h"
#include "typeaux.h"
#include "xtutils.h"
#include "rtc.h"
#include "dport.h"
#include "romfunctions.h"

// ============== Defines ==============
#define SYSCON_PRE_DIV_CNT 0
#define SYSCON_PRE_DIV_CNT_OFS 0U
#define SYSCON_PRE_DIV_CNT_BITS 10U

#define SYSCON_XTAL_TICK_NUM 0
#define SYSCON_XTAL_TICK_NUM_OFS 0
#define SYSCON_XTAL_TICK_NUM_BITS 8

#define RTC_CNTL_SOC_CLK_SEL 0 // use as PFX
#define RTC_CNTL_SOC_CLK_SEL_OFS 27U
#define RTC_CNTL_SOC_CLK_SEL_BITS 2U

#define DPORT_CPU_CPUPERIOD_SEL 0 // use as PFX
#define DPORT_CPU_CPUPERIOD_SEL_OFS 0U
#define DPORT_CPU_CPUPERIOD_SEL_BITS 2U

#define RTC_CNTL_DIG_VREG_DBIAS_WAK 0  // use as PFX
#define RTC_CNTL_DIG_VREG_DBIAS_WAK_OFS 11
#define RTC_CNTL_DIG_VREG_DBIAS_WAK_BITS 3

#define DBIAS_1V00 2U
#define DBIAS_1V10 4U
#define DBIAS_1V25 7U

#define I2C_BBPLL_ADDR_LREF 2         // div_ref, div10_8, lref
#define I2C_BBPLL_ADDR_DIV7_0 3       // div7_0
#define I2C_BBPLL_ADDR_DCUR 5         // dcur, bw
#define I2C_BBPLL_ADDR_ENDIV5 11
#define I2C_BBPLL_ADDR_BBADC_DSNP 9

#define I2CBBPLL_DIVREF_ADDR I2C_BBPLL_ADDR_LREF
#define I2CBBPLL_DIVREF_OFS 0
#define I2CBBPLL_DIVREF_BITS 4
#define I2CBBPLL_DIV7_0_ADDR I2C_BBPLL_ADDR_DIV7_0
#define I2CBBPLL_DIV7_0_OFS 0
#define I2CBBPLL_DIV7_0_BITS 8
#define I2CBBPLL_DIV10_8_ADDR I2C_BBPLL_ADDR_LREF
#define I2CBBPLL_DIV10_8_OFS 4
#define I2CBBPLL_DIV10_8_BITS 3
#define I2CBBPLL_LREF_ADDR I2C_BBPLL_ADDR_LREF
#define I2CBBPLL_LREF_OFS 7
#define I2CBBPLL_LREF_BITS 1
#define I2CBBPLL_DCUR_ADDR I2C_BBPLL_ADDR_DCUR
#define I2CBBPLL_DCUR_OFS 0
#define I2CBBPLL_DCUR_BITS 3
#define I2CBBPLL_BW_ADDR I2C_BBPLL_ADDR_DCUR
#define I2CBBPLL_BW_OFS 6
#define I2CBBPLL_BW_BITS 2

// ============== Local types ==============

typedef struct {
  uint8_t divRef; // 4bits
  uint8_t div7_0;
  uint8_t div10_8;
  uint8_t lref;
  uint8_t dcur;
  uint8_t bw;
  uint8_t endiv5;
  uint8_t bbadcDsmpVal;
} SPllValues;

// ============== Local module-wide variables ==============
static const SPllValues gasBbPllValues[] = {
  {0, 32, 0, 0, 6, 3, 0x43, 0x84},  // 320/160MHz
  {0, 28, 0, 0, 6, 3, 0xC3, 0x74}   // 480MHz
};

// ============== Internal function declarations ==============
// efuse register should be read only once (as it does not change its value)

static uint8_t _dbias240mhz() {
  static bool bFirst = true;
  static uint8_t u8Ret;
  if (bFirst) {
    bFirst = false;
    u8Ret = DBIAS_1V25 - FIELD_GET(gsEFUSE.BLK0RDATA[EFUSE_VOL_LVL_HP_INV_BLK0REG], EFUSE_VOL_LVL_HP_INV);
  }
  return u8Ret;
}
// ============== Implementation ==============
// -------------- Internal functions --------------

// -------------- Interface functions --------------

uint8_t clock_get_dig_vreg_dbias_wak() {
  return FIELD_GET(gsRTC.VREG, RTC_CNTL_DIG_VREG_DBIAS_WAK);
}

void clock_set_dig_vreg_dbias_wak(uint8_t u8DigDbias) {
  gsRTC.VREG = FIELD_REPLACE(gsRTC.VREG, u8DigDbias, RTC_CNTL_DIG_VREG_DBIAS_WAK);
}

ECpuClockSource clock_get_cpuclksrc() {
  return FIELD_GET(gsRTC.CLK_CONF, RTC_CNTL_SOC_CLK_SEL);
}

EPllCpuClockFreq clock_get_pllcpufreq() {
  return FIELD_GET(gsDPORT.CPU_PER_CONF, DPORT_CPU_CPUPERIOD_SEL);
};

void clock_switch_to_xtl(uint16_t u16Divisor, uint8_t u8RefDivisor) {
  gprSYSCON[0] = FIELD_REPLACE(gprSYSCON[0], u16Divisor - 1, SYSCON_PRE_DIV_CNT);
  gprSYSCON[1] = FIELD_REPLACE(gprSYSCON[1], u8RefDivisor - 1, SYSCON_XTAL_TICK_NUM);
  gsRTC.CLK_CONF = FIELD_REPLACE(gsRTC.CLK_CONF, CLK_XTL, RTC_CNTL_SOC_CLK_SEL);
  uint8_t u8DigDBias = (1 < u16Divisor) ? DBIAS_1V00 : DBIAS_1V10;
  clock_set_dig_vreg_dbias_wak(u8DigDBias);
}

void clock_switch_to_pll(EPllCpuClockFreq eFreq) {
  // get current clk src
  ECpuClockSource eClkSrc = clock_get_cpuclksrc();

  // set XTAL as clk src
  if (eClkSrc != CLK_XTL) {
    gsRTC.CLK_CONF = FIELD_REPLACE(gsRTC.CLK_CONF, CLK_XTL, RTC_CNTL_SOC_CLK_SEL);
    esp_rom_delay_us(1);
  }

  // set voltage
  {
    uint8_t u8DigDBias = (eFreq < PLLCPUFREQ_240MHZ) ? DBIAS_1V10 : _dbias240mhz();
    clock_set_dig_vreg_dbias_wak(u8DigDBias);
  }

  esp_rom_delay_us(1);

  // bbpll configuration
  {
    const SPllValues *psVals = &gasBbPllValues[(eFreq < PLLCPUFREQ_240MHZ) ? 0 : 1];
    // div_ref, div10_8, lref
    uint8_t lref = FIELD_MASKNSHIFT(psVals->divRef, I2CBBPLL_DIVREF);
    lref |= FIELD_MASKNSHIFT(psVals->div10_8, I2CBBPLL_DIV10_8);
    lref |= FIELD_MASKNSHIFT(psVals->lref, I2CBBPLL_LREF);
    esp_rom_regi2c_write(0x66, 4, I2C_BBPLL_ADDR_LREF, lref);

    uint8_t div7_0 = FIELD_MASKNSHIFT(psVals->div7_0, I2CBBPLL_DIV7_0);
    esp_rom_regi2c_write(0x66, 4, I2C_BBPLL_ADDR_DIV7_0, div7_0);

    uint8_t dcur = FIELD_MASKNSHIFT(psVals->dcur, I2CBBPLL_DCUR);
    dcur |= FIELD_MASKNSHIFT(psVals->bw, I2CBBPLL_BW);
    esp_rom_regi2c_write(0x66, 4, I2C_BBPLL_ADDR_DCUR, dcur);

    esp_rom_regi2c_write(0x66, 4, I2C_BBPLL_ADDR_ENDIV5, psVals->endiv5);
    esp_rom_regi2c_write(0x66, 4, I2C_BBPLL_ADDR_BBADC_DSNP, psVals->bbadcDsmpVal);
  }
  esp_rom_delay_us(1);

  // set frequency
  gsDPORT.CPU_PER_CONF = FIELD_REPLACE(gsDPORT.CPU_PER_CONF, eFreq, DPORT_CPU_CPUPERIOD_SEL);

  esp_rom_delay_us(1);

  // set PLL as clk src
  gsRTC.CLK_CONF = FIELD_REPLACE(gsRTC.CLK_CONF, CLK_PLL, RTC_CNTL_SOC_CLK_SEL);
}
