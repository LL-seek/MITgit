#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef struct                                      //定义CAN原始接收帧
{
    CAN_RxHeaderTypeDef header;                      //保存标识符、帧类型和数据长度等报文头信息
    uint8_t data[8];                                 //保存CAN报文的8字节数据区
} CAN_RxFrame;

HAL_StatusTypeDef BSP_CAN_SetFilter(uint16_t can_id); //配置本机CAN ID对应的接收过滤器
uint8_t BSP_CAN_Read(CAN_RxFrame *msg);               //供主循环取帧，返回1表示取到一帧，0表示没有待取帧
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

#endif