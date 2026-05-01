#include "lockmgr.h"
#include <stdlib.h>

//void lockmgr_init();
bool lockmgr_acquire_lock(ELockmgrResource eBus, uint32_t *pu32Label) {
  return true;
}

//bool lockmgr_is_locked(ELockmgrResource eBus);
//uint32_t lockmgr_get_lock_owner(ELockmgrResource eBus);
void lockmgr_free_lock(ELockmgrResource eBus) {
  
}

AsyncResultEntry *lockmgr_get_entry(uint32_t u32Label) {
  return NULL;
}
void lockmgr_release_entry(uint32_t u32Label) {
  
}
