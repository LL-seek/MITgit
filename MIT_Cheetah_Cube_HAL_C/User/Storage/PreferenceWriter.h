#ifndef PREFERENCE_WRITER_H
#define PREFERENCE_WRITER_H

#include "motor_types.h"
#include "FlashWriter.h"

void PreferenceWriter_Decode(
    MotorParameters *parameters,        //接收转换后的电机参数
    const int32_t int_reg[256],         //按旧格式读取的256个整数参数
    const float float_reg[64]);         //按旧格式读取的64个浮点参数
		
void PreferenceWriter_Validate(MotorParameters *parameters);  //规处理启动参数

void PreferenceWriter_Load(MotorParameters *parameters);      //从Flash加载原工程参数

HAL_StatusTypeDef PreferenceWriter_Flush(const MotorParameters *parameters);                 //将当前参数按原工程布局写入Flash
#endif
		
		
		