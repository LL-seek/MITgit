#ifndef PWM_H
#define PWM_H

#include <stdint.h>                                          

void PWM_SetCompare(uint32_t compare_u,                      //U相TIM1比较计数值
                    uint32_t compare_v,                      //V相TIM1比较计数值
                    uint32_t compare_w);                     //W相TIM1比较计数值

#endif                                                       




