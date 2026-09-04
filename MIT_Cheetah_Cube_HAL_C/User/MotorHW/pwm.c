#include "pwm.h"                                             //包含PWM接口声明
#include "tim.h"                                             //包含TIM1句柄和HAL定时器接口

void PWM_SetCompare(uint32_t compare_u,
                    uint32_t compare_v,
                    uint32_t compare_w)                      //将U/V/W三相比较计数值写入TIM1对应通道
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, compare_u); //将U相比较值写入CCR3，对应TIM1_CH3/PA10
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, compare_v); //将V相比较值写入CCR2，对应TIM1_CH2/PA9
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_w); //将W相比较值写入CCR1，对应TIM1_CH1/PA8
}





