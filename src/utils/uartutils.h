/*
 * Copyright 2025 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef UARTUTILS_H
#define UARTUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uart.h"

  int uart_printf(UART_Type *psUART, const char *pcFormat, ...);

#ifdef __cplusplus
}
#endif

#endif /* UARTUTILS_H */

