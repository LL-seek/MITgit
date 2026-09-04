#include "pwm.h"                                             //包含PWM接口声明
#include "tim.h"                                             //包含TIM1句柄和HAL定时器接口
#include "hw_config.h"                                       //包含占空比范围、安全中点和PWM周期配置

void PWM_SetCompare(float duty_u,float duty_v,float duty_w)                            //将U/V/W三相占空比转换为TIM1比较值并写入对应通道
{
    uint32_t compare_u;                                      //保存U相TIM1比较计数值
    uint32_t compare_v;                                      //保存V相TIM1比较计数值
    uint32_t compare_w;                                      //保存W相TIM1比较计数值

    if (!((duty_u >= DTC_MIN) && (duty_u <= DTC_MAX)) ||     //检查U相占空比是否在允许范围内
        !((duty_v >= DTC_MIN) && (duty_v <= DTC_MAX)) ||     //检查V相占空比是否在允许范围内
        !((duty_w >= DTC_MIN) && (duty_w <= DTC_MAX)))       //检查W相占空比是否在允许范围内
    {
        duty_u = DTC_SAFE;                                   //任意输入非法时将U相恢复为安全中点
        duty_v = DTC_SAFE;                                   //任意输入非法时将V相恢复为安全中点
        duty_w = DTC_SAFE;                                   //任意输入非法时将W相恢复为安全中点
    }

    compare_u = (uint32_t)(((1.0f - duty_u) * (float)PWM_ARR) + 0.5f); //按照PWM Mode 2将U相占空比转换为比较值
    compare_v = (uint32_t)(((1.0f - duty_v) * (float)PWM_ARR) + 0.5f); //按照PWM Mode 2将V相占空比转换为比较值
    compare_w = (uint32_t)(((1.0f - duty_w) * (float)PWM_ARR) + 0.5f); //按照PWM Mode 2将W相占空比转换为比较值

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, compare_u);  //将U相比较值写入CCR3，对应TIM1_CH3/PA10
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, compare_v);  //将V相比较值写入CCR2，对应TIM1_CH2/PA9
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_w);  //将W相比较值写入CCR1，对应TIM1_CH1/PA8
}