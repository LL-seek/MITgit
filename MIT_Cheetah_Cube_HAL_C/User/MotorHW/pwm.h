#ifndef PWM_H
#define PWM_H

#include <stdint.h>   
#include "stm32f4xx_hal.h"

void PWM_SetCompare(float duty_u,
                    float duty_v,
                    float duty_w);                           //将U/V/W三相占空比转换为TIM1比较值并写入对应通道
										
										
										
HAL_StatusTypeDef PWM_Start(void);    //以安全CCR启动TIM1三相PWM和更新中断，随后退出DRV的COAST状态，返回HAL状态
HAL_StatusTypeDef PWM_Stop(void);     //先使DRV进入COAST状态，再停止TIM1三相PWM和更新中断，恢复安全CCR并返回HAL状态

#endif                                                       


