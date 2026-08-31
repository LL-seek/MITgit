#include "bsp_emergency_stop.h"                         
#include "bsp_safe_gpio.h"                           
#include "main.h"                                       


static volatile bool s_emergency_active = false;        //急停状态锁存标志
static volatile BSP_ErrorCode s_error_code = BSP_ERROR_NONE; //第一次急停的错误码


void BSP_EmergencyStop(BSP_ErrorCode error_code)        //执行统一急停并锁存错误
{
    uint32_t saved_primask = __get_PRIMASK();            //保存进入函数前的中断状态

    __disable_irq();                                     //防止急停过程被中断打断

    if (!s_emergency_active)                             //只处理第一次急停
    {
        s_error_code = error_code;                       //保存第一次急停错误码
        s_emergency_active = true;                       //锁存急停状态
    }

    BSP_SafeGpioEarlyInit();                             //拉低 ENABLE 和 PWM，并拉高所有 CS

    __HAL_RCC_TIM1_CLK_ENABLE();                         //开启 TIM1 时钟以访问寄存器

    TIM1->BDTR &= ~TIM_BDTR_MOE;                         //关闭 TIM1 主输出
    TIM1->CCER = 0U;                                     //关闭 TIM1 所有通道输出
    TIM1->DIER = 0U;                                     //禁止 TIM1 中断和 DMA 请求
    TIM1->CR1 &= ~TIM_CR1_CEN;                           //停止 TIM1 计数器

    __DSB();                                             //等待寄存器写入完成

    __set_PRIMASK(saved_primask);                        //恢复进入函数前的中断状态
}


bool BSP_EmergencyStop_IsActive(void)                    //查询急停是否已经触发
{
    return s_emergency_active;                           //返回急停锁存状态
}


BSP_ErrorCode BSP_EmergencyStop_GetErrorCode(void)       //读取第一次急停错误码
{
    return s_error_code;                                 //返回已保存的错误码
}