/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef LOCKMGR_H
#define LOCKMGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

  typedef enum {
    LOCKMGR_I2C0 = 0,
    LOCKMGR_I2C1 = 1,
    LOCKMGR_RESOURCES
  } ELockmgrResource;

  typedef struct {
    uint8_t u8RxLen : 8;
    bool bActive : 1;
    bool bReady : 1;
    uint32_t resv10 : 22;
    uint32_t u32Label;
    uint32_t u32IntSt;
    uint8_t *pu8ReceiveBuffer;
  } AsyncResultEntry;

  void lockmgr_init();
  bool lockmgr_acquire_lock(ELockmgrResource eBus, uint32_t *pu32Label);
  bool lockmgr_is_locked(ELockmgrResource eBus);
  uint32_t lockmgr_get_lock_owner(ELockmgrResource eBus);
  void lockmgr_free_lock(ELockmgrResource eBus);
  AsyncResultEntry *lockmgr_get_entry(uint32_t u32Label);
  void lockmgr_release_entry(uint32_t u32Label);

#ifdef __cplusplus
}
#endif

#endif /* LOCKMGR_H */
