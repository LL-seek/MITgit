#include "bsp_debug_uart.h"                             
#include "usart.h"                                       

#include <stddef.h>                                      //提供 NULL 定义
#include <stdint.h>                                      //提供 uint8_t 和 uint16_t 类型

#define BSP_DEBUG_UART_TX_CAPACITY  128U                 //单次最大发送字节数

static uint8_t s_tx_buffer[BSP_DEBUG_UART_TX_CAPACITY];  //UART 静态发送缓冲区
static volatile bool s_tx_busy = false;                  //UART 发送忙状态标志

bool BSP_DebugUart_TryWrite(const char *text)             //尝试通过 USART2 非阻塞发送字符串
{
    uint16_t length = 0U;                                //记录字符串长度
    uint16_t index;                                      //复制字符串时使用的索引

    if ((text == NULL) || (__get_IPSR() != 0U))          //检查字符串地址以及是否处于中断中
    {
        return false;                                    //参数无效或处于中断中，拒绝发送
    }

    if (s_tx_busy)                                       //检查上一次发送是否完成
    {
        return false;                                    //UART 正忙，直接返回
    }

    while ((length < BSP_DEBUG_UART_TX_CAPACITY) &&      //限制检查长度不超过缓冲区容量
           (text[length] != '\0'))                       //检查是否到达字符串结束符
    {
        length++;                                        //检查下一个字符
    }

    if (text[length] != '\0')                            //检查前 128 字节后字符串是否结束
    {
        return false;                                    //字符串过长，拒绝发送
    }

    if (length == 0U)                                    //检查是否为空字符串
    {
        return true;                                     //没有内容需要发送
    }

    s_tx_busy = true;                                    //标记 UART 正在发送

    for (index = 0U; index < length; index++)             
    {
        s_tx_buffer[index] = (uint8_t)text[index];        //将字符复制到静态发送缓冲区
    }

    if (HAL_UART_Transmit_IT(&huart2,                     //指定使用 USART2
                             s_tx_buffer,                 //指定发送缓冲区
                             length) != HAL_OK)           //启动 UART 中断发送并检查结果
    {
        s_tx_busy = false;                               //启动失败，恢复 UART 空闲状态
        return false;                                    //返回发送失败
    }

    return true;                                         //UART 已接受本次发送任务
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)   //UART 发送完成回调函数
{
    if (huart == &huart2)                                //检查完成发送的是否为 USART2
    {
        s_tx_busy = false;                               //发送完成，恢复 UART 空闲状态
    }
}