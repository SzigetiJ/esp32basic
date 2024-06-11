/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stddef.h>
#include <stdint.h>

#include "i2c.h"
#include "lockmgr.h"
#include "typeaux.h"
#include "xtutils.h"

#define LOCKMGR_STORE_SIZE 10U  ///< Size of the (shared) result store.

// ================= Local module-wide variables ==================
static volatile AsyncResultEntry gasResult[LOCKMGR_STORE_SIZE]; ///< Result entries are being stored here.
static volatile uint32_t gau32Mutex[LOCKMGR_RESOURCES]; ///< Mutex (per resource).
static volatile uint32_t gau32LastAssignedLabel[LOCKMGR_RESOURCES]; ///< Labels get assigned incrementally, so we have to know what is the last assigned value (per resource).

// ================= Local function declarations ==================
static void _alloc_entry(uint32_t u32EntryIdx, uint32_t u32Label);
static void _free_entry(uint32_t u32EntryIdx);
static bool _find_free_entry(uint32_t *pu32EntryIdx);
static bool _find_entry(uint32_t *pu32EntryIdx, uint32_t u32Label);

// ================= ENTRY basic methods ==========================

/**
 * Marks an entry in the result store as allocated (used), binds it to a label
 * and initializes/resets the content of the entry.
 * @param u32EntryIdx Index of the entry.
 * @param u32Label Label of the entry (to assign).
 */
static void _alloc_entry(uint32_t u32EntryIdx, uint32_t u32Label) {
  gasResult[u32EntryIdx].bReady = false;
  gasResult[u32EntryIdx].u32Label = u32Label;
  gasResult[u32EntryIdx].bActive = true;
  gasResult[u32EntryIdx].u8RxLen = 0;
}

/**
 * Marks an entry in the result store as free (unused).
 * @param u32EntryIdx Index of the entry.
 */
static void _free_entry(uint32_t u32EntryIdx) {
  gasResult[u32EntryIdx].bActive = false;
}

/**
 * Finds an unused entry in result store and returns its index (output parameter).
 * @param pu32EntryIdx (out) Index of the first free (unused) entry in the store.
 * @return Success.
 */
static bool _find_free_entry(uint32_t *pu32EntryIdx) {
  for (uint32_t i = 0; i < ARRAY_SIZE(gasResult); ++i) {
    if (!gasResult[i].bActive) {
      *pu32EntryIdx = i;
      return true;
    }
  }
  return false;
}

/**
 * Finds entry by label.
 * @param pu32EntryIdx (out) Index of the entry.
 * @param u32Label (in) Label of the entry to search for.
 * @return Success.
 */
static bool _find_entry(uint32_t *pu32EntryIdx, uint32_t u32Label) {
  for (uint32_t i = 0; i < ARRAY_SIZE(gasResult); ++i) {
    if (gasResult[i].bActive && gasResult[i].u32Label == u32Label) {
      *pu32EntryIdx = i;
      return true;
    }
  }
  return false;
}

// ====================== driver functions ======================

/**
 * Initializes the static variables of the lockmgr module.
 */
void lockmgr_init() {
  for (uint32_t i = 0; i < ARRAY_SIZE(gasResult); ++i) {
    gasResult[i].bActive = false;
  }
  for (int i = 0; i < LOCKMGR_RESOURCES; ++i) {
    gau32Mutex[i] = 0;
    gau32LastAssignedLabel[i] = -1;
  }
}

// =============== LOCKS =================

/**
 * Acquires a lock for a given resource (if available). In case of successful
 * allocation and if possible, reserves a result entry.
 * @param eBus Identifies the resource.
 * @param pu32Label (out) Label that can be used to access the result entry.
 * @return Success.
 */
bool lockmgr_acquire_lock(ELockmgrResource eBus, uint32_t *pu32Label) {
  static uint32_t u32LabelCursor = 0;
  uint32_t u32CoreId = xt_utils_get_core_id();
  bool bLockAcquired = xt_utils_compare_and_set(&gau32Mutex[eBus], 0, u32CoreId + 1);
  if (bLockAcquired) {
    uint32_t u32EntryIdx;
    bool bRes = _find_free_entry(&u32EntryIdx);
    if (!bRes) { // some exception..
      lockmgr_free_lock(eBus);
      return false;
    }
    *pu32Label = u32LabelCursor++;
    _alloc_entry(u32EntryIdx, *pu32Label);
    gau32LastAssignedLabel[eBus] = *pu32Label;
  }
  return bLockAcquired;
}

/**
 * Tells whether a resource is locked or not.
 * @param eBus Identifies the resource.
 * @return The resource is locked.
 */
bool lockmgr_is_locked(ELockmgrResource eBus) {
  return gau32Mutex[eBus] != 0;
}

/**
 * Gets the owner (label) of the lock.
 * @param eBus Identifies the resource.
 * @return Owner of the lock (undefined if the resource is not locked).
 */
uint32_t lockmgr_get_lock_owner(ELockmgrResource eBus) {
  return gau32LastAssignedLabel[eBus];
}

/**
 * Releases a resource lock.
 * @param eBus Identifies the resource.
 */
void lockmgr_free_lock(ELockmgrResource eBus) {
  gau32Mutex[eBus] = 0;
}

// =============== ENTRY =================

/**
 * Entry lookup by label. Only allocated (used) entries count.
 * @param u32Label Label to search for.
 * @return Reference to the entry with the given label (NULL if not found).
 */
AsyncResultEntry *lockmgr_get_entry(uint32_t u32Label) {
  uint32_t u32EntryIdx;
  bool bRes = _find_entry(&u32EntryIdx, u32Label);
  return bRes ? (AsyncResultEntry *) & gasResult[u32EntryIdx] : NULL;
}

/**
 * Releases an entry (identified by label).
 * @param u32Label Label to search for.
 */
void lockmgr_release_entry(uint32_t u32Label) {
  uint32_t u32EntryIdx;
  bool bRes = _find_entry(&u32EntryIdx, u32Label);
  if (bRes) {
    _free_entry(u32EntryIdx);
  }
}
