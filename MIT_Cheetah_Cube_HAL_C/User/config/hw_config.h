#ifndef HW_CONFIG_H                              
#define HW_CONFIG_H                              

#define I_SCALE  0.02014160156f                  //ADC电流换算比例， A/count
#define V_SCALE  0.012890625f                    //ADC母线电压换算比例， V/count

#define DTC_MAX   0.94f   //PWM允许的最大占空比为94%
#define DTC_MIN   0.0f    //PWM允许的最小占空比为0%
#define DTC_SAFE  0.5f    //三相零转矩时的安全中点占空比为50%

#define PWM_ARR  0x8CA                           //TIM1自动重装值，十进制为 2250
#define OVERMODULATION 1.15f                     //弱磁计算使用的过调制系数

#endif                                           










