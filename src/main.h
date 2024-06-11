/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  extern const bool gbStartAppCpu;
  extern const uint16_t gu16Tim00Divisor;
  extern const uint64_t gu64tckSchedulePeriod;

  void prog_init_pro_pre();
  void prog_init_app();
  void prog_init_pro_post();

  void prog_cycle_pro(uint64_t u64tckCur);
  void prog_cycle_app(uint64_t u64tckCur);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
