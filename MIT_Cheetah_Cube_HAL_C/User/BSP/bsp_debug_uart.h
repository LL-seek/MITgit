#ifndef BSP_DEBUG_UART_H
#define BSP_DEBUG_UART_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool BSP_DebugUart_TryWrite(const char *text);

#ifdef __cplusplus
}
#endif

#endif
