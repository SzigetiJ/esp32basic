/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */

#include <stdarg.h>
#include <stdio.h>
#ifdef __XTENSA__
#include <sys/reent.h>
#if __GNUC__ >= 13
#define IMPURE_PTR _impure_ptr
#else
#define IMPURE_PTR _global_impure_ptr
#endif
#else
#define _impure_ptr NULL
#define _vsnprintf_r(X,Y1,Y2,Y3,Y4) vsnprintf(Y1,Y2,Y3,Y4)
#endif
#include <stdlib.h>
#include "uartutils.h"
#include "typeaux.h"

int uart_printf(UART_Type *psUART, const char *pcFormat, ...) {
  char acBuf[100];
  va_list va;
  va_start(va, pcFormat);
  int iLen = _vsnprintf_r(IMPURE_PTR, acBuf, ARRAY_SIZE(acBuf), pcFormat, va);
  va_end(va);
  for (int i = 0; i < iLen; ++i) {
    psUART->FIFO = acBuf[i];
  }
  return iLen;
}

