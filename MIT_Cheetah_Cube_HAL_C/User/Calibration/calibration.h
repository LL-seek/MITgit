#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "motor_types.h"

#define V_CAL 0.08f                                      //沿用原工程的标定电压指令

void order_phases(const volatile PositionSnapshot *ps,   //读取现有中断更新的编码器采样结果
                  const volatile ControllerStruct *controller,
                  MotorParameters *parameters);         //将相序结果写入原有参数结构体

void calibrate_encoder(const volatile PositionSnapshot *ps, //原工程calibrate，改名以避开已有DRV同名C函数
                       MotorParameters *parameters);    //计算电角度偏置和128项LUT，保存由主循环完成

#endif
