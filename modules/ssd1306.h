/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

  // ============== Defines ==============

  // ============== Types ==============

  typedef enum {
    SSD1306_ADDR_HORZ = 0,
    SSD1306_ADDR_VERT = 1,
    SSD1306_ADDR_PAGE = 2,
    SSD1306_ADDR_INVALID = 3
  } EAddressingMode;

  // ============== Inline interface functions ==============

  // ============== Interface functions ==============
  uint8_t ssd1306_ctrl(uint8_t *pu8Dest, bool bContinuate, bool bData);
  uint8_t ssd1306_get_startseq(uint8_t *pu8Dest);
  uint8_t ssd1306_inverse_display(uint8_t *pu8Dest, bool bInverse);
  uint8_t ssd1306_set_contrast_control(uint8_t *pu8Dest, uint8_t u8Value);
  uint8_t ssd1306_set_display_offset(uint8_t *pu8Dest, uint8_t u8Offset);
  uint8_t ssd1306_set_display_startline(uint8_t *pu8Dest, uint8_t u8StartLine);
  uint8_t ssd1306_set_hv_column_range(uint8_t *pu8Dest, uint8_t u8First, uint8_t u8Last);
  uint8_t ssd1306_set_hv_page_range(uint8_t *pu8Dest, uint8_t u8First, uint8_t u8Last);
  uint8_t ssd1306_set_memory_addressing_mode(uint8_t *pu8Dest, EAddressingMode eMode);
  uint8_t ssd1306_set_mux_ratio(uint8_t *pu8Dest, uint8_t u8Ratio);
  uint8_t ssd1306_set_page_page(uint8_t *pu8Dest, uint8_t u8Page);
  uint8_t ssd1306_set_page_start_column(uint8_t *pu8Dest, uint8_t u8Start);
  uint8_t ssd1306_set_segment_remap(uint8_t *pu8Dest, bool bReverse);
  uint8_t ssd1306_set_output_scan_dir(uint8_t *pu8Dest, bool bReverse);

  uint8_t ssd1306_set_com_pins_hw_config(uint8_t *pu8Dest, bool bAltPinCfg, bool bComLRRemap);
  uint8_t ssd1306_entire_display_on(uint8_t *pu8Dest, bool bOn);
  uint8_t ssd1306_set_display_on(uint8_t *pu8Dest, bool bOn);
  uint8_t ssd1306_set_display_osc_freq(uint8_t *pu8Dest, uint8_t u8Freq, uint8_t u8Divisor);
  uint8_t ssd1306_set_precharge_period(uint8_t *pu8Dest, uint8_t u8dclkPhase1Len, uint8_t u8dclkPhase2Len);
  uint8_t ssd1306_charge_pump_setting(uint8_t *pu8Dest, bool bEnable);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
