#ifndef PWM_H
#define PWM_H

#include <stdint.h>                                          

void PWM_SetCompare(float duty_u,
                    float duty_v,
                    float duty_w);                           //将U/V/W三相占空比转换为TIM1比较值并写入对应通道

#endif                                                       


