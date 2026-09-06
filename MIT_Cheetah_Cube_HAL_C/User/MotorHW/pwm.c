#include "pwm.h"                                             //包含PWM接口声明
#include "tim.h"                                             //包含TIM1句柄和HAL定时器接口
#include "hw_config.h"                                       //包含占空比范围、安全中点和PWM周期配置
#include "DRV.h"

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


HAL_StatusTypeDef PWM_Stop(void)                              //先使DRV进入COAST，再停止TIM1三相PWM，恢复安全CCR并返回HAL状态
{
    HAL_StatusTypeDef status;                                //保存DRV进入COAST时的HAL通信状态

    status = disable_gd();                                   //设置DRV8323的COAST位，先关闭功率级输出

    HAL_TIM_Base_Stop_IT(&htim1);                      //关闭TIM1更新中断，并将HAL定时器状态恢复为READY

    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);           //停止TIM1通道1，对应W相PWM输出
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);           //停止TIM1通道2，对应V相PWM输出
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);           //停止TIM1通道3，对应U相PWM输出

    PWM_SetCompare(DTC_SAFE, DTC_SAFE, DTC_SAFE);            //将U、V、W三相比较值恢复为安全中点

    __HAL_TIM_SET_COUNTER(&htim1, 0U);                       //将TIM1计数器清零，为下一次启动准备确定的起点
    HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE);   //产生软件更新事件，将预装载的安全CCR写入活动寄存器
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);           //清除软件更新产生的更新标志，避免下次启动触发残留中断

    return status;                                           //返回本次设置DRV进入COAST时的HAL通信状态
}


HAL_StatusTypeDef PWM_Start(void)                            //以安全CCR启动TIM1三相PWM和更新中断，最后退出DRV的COAST状态
{
    HAL_StatusTypeDef status;                                //保存PWM启动过程和DRV通信的HAL状态

	  HAL_TIM_Base_Stop_IT(&htim1);
    PWM_SetCompare(DTC_SAFE, DTC_SAFE, DTC_SAFE);            //将U、V、W三相比较值设置为安全中点
    __HAL_TIM_SET_COUNTER(&htim1, 0U);                       //将TIM1计数器清零，使PWM从确定的计数位置启动

    status = HAL_TIM_GenerateEvent(&htim1,                   //产生TIM1软件更新事件，将预装载的安全CCR写入活动寄存器
                                   TIM_EVENTSOURCE_UPDATE);
    if (status != HAL_OK)                                    //软件更新事件产生失败时，不继续启动PWM
    {
        return status;                                       //返回TIM1软件更新事件的失败状态
    }

    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);           //清除软件更新产生的更新标志，避免启动时触发残留中断

    status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);       //启动TIM1通道1，对应W相PWM输出

    if (status == HAL_OK)                                    //W相PWM启动成功后，继续启动V相PWM
    {
        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);   //启动TIM1通道2，对应V相PWM输出
    }

    if (status == HAL_OK)                                    //V相PWM启动成功后，继续启动U相PWM
    {
        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);   //启动TIM1通道3，对应U相PWM输出
    }

    if (status == HAL_OK)                                    //三相PWM均启动成功后，启动TIM1更新中断
    {
        __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);       //再次清除更新标志，避免使能中断后立即响应旧标志
        status = HAL_TIM_Base_Start_IT(&htim1);              //启动TIM1更新中断，建立40kHz控制时基
    }

    if (status == HAL_OK)                                    //三相PWM和更新中断均启动成功后，才接通功率级
    {
        status = enable_gd();                                //清除DRV8323的COAST位，允许栅极驱动输出
    }

    if (status != HAL_OK)                                    //PWM、更新中断或DRV启动过程中任一步失败时执行安全停止
    {
        PWM_Stop();                                    //使DRV进入COAST，并停止已启动的TIM1通道和更新中断
    }

    return status;                                           //返回本次PWM启动过程的最终HAL状态
}













