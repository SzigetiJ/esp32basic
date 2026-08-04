/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef ROMFUNCTIONS_H
#define ROMFUNCTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifdef __XTENSA__
#define RSR(reg, curval) __asm__ volatile ("rsr %0, " #reg : "=r" (curval));
#define WSR(reg, newval) __asm__ volatile ("wsr %0, " #reg : : "r" (newval));
#else
#define RSR(reg, curval) while(0){};
#define WSR(reg, curval) while(0){};
#endif
  
  void ets_isr_mask(uint32_t mask);
  void ets_isr_unmask(uint32_t mask);
  void esp_rom_delay_us(uint32_t u32Delay);
  void *_xtos_set_interrupt_handler(int irq_number, void* function);
  void *_xtos_set_interrupt_handler_arg(int irq_number, void* function, int argument);

  void gpio_matrix_out(uint32_t gpio, uint32_t signal_idx, bool out_inv, bool oen_inv);
  void gpio_matrix_in(uint32_t gpio, uint32_t signal_idx, bool inv);

  /* get CCOUNT register (if not present return 0) */
  extern uint32_t xthal_get_ccount(void);

  /* set and get CCOMPAREn registers (if not present, get returns 0) */
  extern void     xthal_set_ccompare(int iReg, uint32_t u32Value);
  extern uint32_t xthal_get_ccompare(int iReg);
  void            xthal_set_intclear(uint32_t u32IntMask);

// taken from ESP-IDF (v5.5.2) esp_rom_regi2c.h
  // (block, hostid) pair is (0x66, 4) for BBPLL and (0x6D, 3) for audio PLL
uint8_t esp_rom_regi2c_read(uint8_t block, uint8_t host_id, uint8_t reg_add);
uint8_t esp_rom_regi2c_read_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb);
void esp_rom_regi2c_write(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
void esp_rom_regi2c_write_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* ROMFUNCTIONS_H */
