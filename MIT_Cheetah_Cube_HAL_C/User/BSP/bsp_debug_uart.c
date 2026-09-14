#include "bsp_debug_uart.h"                             
#include "usart.h"                                       

#include <stddef.h>                                      //提供 NULL 定义
#include <stdint.h>                                      //提供 uint8_t 和 uint16_t 类型

#define BSP_DEBUG_UART_TX_CAPACITY  128U                 //单次最大发送字节数
#define BSP_DEBUG_UART_RX_CAPACITY 64U                         //接收缓冲大小，保留一个位置区分空和满

static uint8_t s_rx_byte;                                      //保存HAL本次接收的一个字节
static volatile uint8_t s_rx_buffer[BSP_DEBUG_UART_RX_CAPACITY]; //保存尚未被主循环取走的字符
static volatile uint16_t s_rx_write = 0U;                       //接收中断的写入位置
static volatile uint16_t s_rx_read = 0U;                        //主循环的读取位置
static volatile bool s_rx_overflow = false;                    //true表示输入发生丢失，false表示未记录丢失

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


bool BSP_DebugUart_StartReceive(void)                          //启动USART2单字节中断接收
{
    return HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U) == HAL_OK;
}




int8_t BSP_DebugUart_Read(char *c)                              //供主循环取出一个字符
{
    int8_t result = 0;
    uint32_t primask = __get_PRIMASK();                        //保存原有中断屏蔽状态

    __disable_irq();                                          //短临界区内处理读取位置和丢失标志

    if (s_rx_overflow)
    {
        s_rx_read = s_rx_write;                               //丢弃当前积压的不完整输入
        s_rx_overflow = false;                                //清除已交给主循环处理的丢失标志
        result = -1;                                         //通知后续输入处理放弃当前命令
    }
    else if (s_rx_read != s_rx_write)
    {
        *c = (char)s_rx_buffer[s_rx_read];                     //取出一个字符
        s_rx_read = (uint16_t)((s_rx_read + 1U) % BSP_DEBUG_UART_RX_CAPACITY);
        result = 1;
    }

    __set_PRIMASK(primask);                                    //恢复原有中断屏蔽状态

    return result;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)          //接收错误后继续接收后续字符
{
    if (huart == &huart2)
    {
        s_rx_overflow = true;                                 //通知主循环当前输入可能不完整
        __HAL_UART_CLEAR_OREFLAG(huart);                       //清除接收溢出等接收状态标志
        (void)HAL_UART_Receive_IT(huart, &s_rx_byte, 1U);       //HAL停止接收时重新启动，仍在接收时返回忙
    }
}


