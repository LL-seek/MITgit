#ifndef FOC_H
#define FOC_H

#include "motor_types.h"

/* 在控制中断内同步调用；无效输入会清除controller->adc.valid。 */
bool FOC_SetAdcSnapshot(ControllerStruct *controller, const AdcSnapshot *sample,const MotorParameters *parameters);

void abc(float theta, float d, float q, float *a, float *b, float *c);                       //将dq分量逆变换为三相a/b/c分量

void dq0(float theta, float a, float b, float c, float *d, float *q);                        //将三相a/b/c分量变换为dq分量

void svm(float v_bus, float u, float v, float w, float *dtc_u, float *dtc_v, float *dtc_w);  //根据三相电压和母线电压计算三相占空比

#endif
