/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_it.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "foc.h"
#include "adc_sample.h"
#include "PositionSensor.h"
#include "math_ops.h"   
#include <math.h>     
#include "hw_config.h"          
#include "pwm.h"                                  
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
static AdcSnapshot adc_sample;
extern ControllerStruct controller;
extern MotorParameters motor_parameters;
extern PositionSnapshot position_sample;
extern FocCommand command;                            
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM1 update interrupt and TIM10 global interrupt.
  */
void TIM1_UP_TIM10_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_TIM10_IRQn 0 */
	if ((__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE) != RESET) &&       
    (__HAL_TIM_GET_IT_SOURCE(&htim1, TIM_IT_UPDATE) != RESET))      
{
ADC_Sample(&adc_sample);    
PositionSensor_Sample(&position_sample,
                      &motor_parameters,
                      0.000025f);                           //按25微秒周期执行位置和速度采样

if (FOC_SetAdcSnapshot(&controller, &adc_sample, &motor_parameters) && position_sample.valid)                         //沿用现有采样有效标志，电流与位置均有效时执行变换
{
	  torque_control(&controller, &command, &position_sample);  //使用本周期位置和速度计算电流参考值
    dq0(position_sample.theta_elec,                   //本周期电角度，rad
        controller.i_a,                               //重构后的A相电流，A
        controller.i_b,                               //重构后的B相电流，A
        controller.i_c,                               //重构后的C相电流，A
        &controller.i_d,                              //保存变换得到的d轴电流，A
        &controller.i_q);                             //保存变换得到的q轴电流，A
	
  	controller.i_q_filt = 0.95f * controller.i_q_filt + 0.05f * controller.i_q;  //保留原工程q轴测量电流滤波，A

    controller.i_d_filt = 0.95f * controller.i_d_filt + 0.05f * controller.i_d;  //保留原工程d轴测量电流滤波，A
	
  	controller.fw_int += 0.001f * (0.5f * OVERMODULATION * controller.v_bus - controller.v_ref);            //根据电压裕量更新弱磁积分，沿用原工程每周期系数

    controller.fw_int = fmaxf(fminf(controller.fw_int, 0.0f), -motor_parameters.I_FW_MAX);                  //将弱磁积分限制在负最大弱磁电流到零之间，A

    controller.i_d_ref = controller.fw_int;                                 //将弱磁积分作为d轴电流参考，A

    limit_norm(&controller.i_d_ref, &controller.i_q_ref,motor_parameters.I_MAX);                            //弱磁更新后统一限制dq电流参考矢量，A
	
	  float i_d_error = controller.i_d_ref - controller.i_d;    //计算d轴电流误差，A
    float i_q_error = controller.i_q_ref - controller.i_q;    //计算q轴电流误差，A

    controller.d_int += controller.k_d * controller.ki_d * i_d_error;  //更新d轴积分电压，V
    controller.q_int += controller.k_q * controller.ki_q * i_q_error;  //更新q轴积分电压，V

    controller.d_int = fmaxf(fminf(controller.d_int, OVERMODULATION * controller.v_bus),-OVERMODULATION * controller.v_bus);  //限制d轴积分电压，V

    controller.q_int = fmaxf(fminf(controller.q_int, OVERMODULATION * controller.v_bus),-OVERMODULATION * controller.v_bus);  //限制q轴积分电压，V

    controller.v_d = controller.k_d * i_d_error + controller.d_int;  //计算d轴PI输出电压，V
    controller.v_q = controller.k_q * i_q_error + controller.q_int;  //计算q轴PI输出电压，V

    controller.v_ref = sqrtf(controller.v_d * controller.v_d +controller.v_q * controller.v_q);  //保存限幅前电压幅值，供下一周期弱磁使用，V

    limit_norm(&controller.v_d, &controller.v_q,OVERMODULATION * controller.v_bus);              //限制dq输出电压矢量，V
		
		float dtc_d = controller.v_d / controller.v_bus;    //将d轴电压归一化为母线电压的比例
    float dtc_q = controller.v_q / controller.v_bus;    //将q轴电压归一化为母线电压的比例

    linearize_dtc(&dtc_d);                             //对d轴归一化电压进行补偿
    linearize_dtc(&dtc_q);                             //对q轴归一化电压进行补偿

    controller.v_d = dtc_d * controller.v_bus;          //将补偿结果还原为d轴电压，V
    controller.v_q = dtc_q * controller.v_bus;          //将补偿结果还原为q轴电压，V
		
		abc(position_sample.theta_elec,controller.v_d,controller.v_q,&controller.v_u,&controller.v_v,&controller.v_w);                              

    svm(controller.v_bus,controller.v_u,controller.v_v,controller.v_w,&controller.dtc_u,&controller.dtc_v,&controller.dtc_w);    
		
		if (motor_parameters.PHASE_ORDER)                  //沿用原工程相序标志非零时的通道对应关系
    {
       PWM_SetCompare(controller.dtc_u,controller.dtc_v,controller.dtc_w);              
    }
    else                                               //相序标志为零时交换V相和W相对应的通道
   {
      PWM_SetCompare(controller.dtc_u,controller.dtc_w,controller.dtc_v);              
   }


}	


}
  /* USER CODE END TIM1_UP_TIM10_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_TIM10_IRQn 1 */

  /* USER CODE END TIM1_UP_TIM10_IRQn 1 */
}

/**
  * @brief This function handles TIM2 global interrupt.
  */
void TIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM2_IRQn 0 */

  /* USER CODE END TIM2_IRQn 0 */
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */

  /* USER CODE END TIM2_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
