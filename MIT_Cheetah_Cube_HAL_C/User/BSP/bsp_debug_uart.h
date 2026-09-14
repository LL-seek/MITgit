#ifndef BSP_DEBUG_UART_H
#define BSP_DEBUG_UART_H

#include <stdbool.h>
#include <stdint.h>
bool BSP_DebugUart_StartReceive(void);       //启动USART2单字节中断接收
int8_t BSP_DebugUart_Read(char *c);           //返回1表示取得字符，0表示无字符，-1表示输入发生丢失

#ifdef __cplusplus
extern "C" {
#endif

bool BSP_DebugUart_TryWrite(const char *text);

#ifdef __cplusplus
}
#endif

#endif
