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

  void ets_isr_unmask(uint32_t mask);
  void _xtos_set_interrupt_handler(int irq_number, void* function);
  void _xtos_set_interrupt_handler_arg(int irq_number, void* function, int argument);

  void gpio_matrix_out(uint32_t gpio, uint32_t signal_idx, bool out_inv, bool oen_inv);
  void gpio_matrix_in(uint32_t gpio, uint32_t signal_idx, bool inv);

#ifdef __cplusplus
}
#endif

#endif /* ROMFUNCTIONS_H */
