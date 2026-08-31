#ifndef HW_CONFIG_H                              
#define HW_CONFIG_H                              

#define I_SCALE  0.02014160156f                  //ADC电流换算比例， A/count
#define V_SCALE  0.012890625f                    //ADC母线电压换算比例， V/count

#define DTC_MAX  0.94f                           //PWM允许的最大占空比为 94%
#define DTC_MIN  0.0f                            //PWM允许的最小占空比为 0%

#define PWM_ARR  0x8CA                           //TIM1自动重装值，十进制为 2250

#endif                                           










