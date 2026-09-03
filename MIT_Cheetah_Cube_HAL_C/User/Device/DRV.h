#ifndef DRV_H                                                              
#define DRV_H                                                               

#include "stm32f4xx_hal.h"                                                   
#include <stdint.h>      


#define FSR1    0x00U                                           //故障状态寄存器1地址
#define FSR2    0x01U                                           //故障状态寄存器2地址
#define DCR     0x02U                                           //驱动控制寄存器地址
#define HSR     0x03U                                           //高侧栅极驱动寄存器地址
#define LSR     0x04U                                           //低侧栅极驱动寄存器地址
#define OCPCR   0x05U                                           //过流保护控制寄存器地址
#define CSACR   0x06U                                           //电流检测放大器控制寄存器地址

HAL_StatusTypeDef read_register(uint8_t reg, uint16_t *val);      //读取寄存器reg，结果写入*val，返回HAL状态
HAL_StatusTypeDef write_register(uint8_t reg, uint16_t val);      //将val写入寄存器reg，返回HAL状态




HAL_StatusTypeDef DRV_Transfer16(uint16_t tx_data, uint16_t *rx_data);          //发送tx_data并将接收值写入rx_data，返回HAL_OK时接收数据有效

#endif     






