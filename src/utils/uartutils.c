/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stdarg.h>
#include <stdio.h>
#include "uartutils.h"
#include "typeaux.h"

extern struct _reent *_global_impure_ptr __ATTRIBUTE_IMPURE_PTR__;

int uart_printf(UART_Type *psUART, const char *pcFormat, ...) {
  char acBuf[100];
  va_list va;
  va_start(va, pcFormat);
  int iLen = _vsnprintf_r(_global_impure_ptr, acBuf, ARRAY_SIZE(acBuf), pcFormat, va);
  va_end(va);
  for (int i = 0; i < iLen; ++i) {
    psUART->FIFO = acBuf[i];
  }
  return iLen;
}

