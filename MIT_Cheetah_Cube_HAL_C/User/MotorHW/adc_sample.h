#ifndef ADC_SAMPLE_H
#define ADC_SAMPLE_H

#include "motor_types.h"                                    

void Init_ADC(void);                                           //使能三路ADC并等待ADC稳定
void ADC_Sample(AdcSnapshot *sample);                          //执行一次三路ADC采样并将结果写入采样快照
bool zero_current(int32_t *offset_1, int32_t *offset_2);       //采集两路电流传感器零偏并计算平均值

#endif                                                        