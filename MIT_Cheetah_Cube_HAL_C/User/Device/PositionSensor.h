#ifndef POSITIONSENSOR_H
#define POSITIONSENSOR_H

#include "stm32f4xx_hal.h"
#include "motor_types.h"



HAL_StatusTypeDef PositionSensor_ReadRaw(GPIO_TypeDef *cs_port,    //编码器片选GPIO端口
                                         uint16_t cs_pin,          //编码器片选GPIO引脚
                                         uint16_t *raw);           //收发角度命令，将低14位写入raw并返回HAL状态
																				 

void PositionSensor_Sample(PositionSnapshot *sample,             //保存本次位置和速度数据
                           const MotorParameters *parameters,   //编码器使用的电机参数
                           float dt);                          //采样周期，单位s												 

void PositionSensor_WriteLUT(const int32_t new_lut[128]);          //将128项编码器校正数据写入模块内部的LUT
void PositionSensor_ResetRotationState(PositionSnapshot *sample);  //用户置零时复位编码器累计圈数和机械位置
HAL_StatusTypeDef PositionSensor_DualEncoder(const MotorParameters *parameters);  //根据双编码器齿差初始化主编码器累计圈数

#endif