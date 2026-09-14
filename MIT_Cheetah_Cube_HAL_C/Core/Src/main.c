/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "adc.h"
#include "can.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_safe_gpio.h"
#include "bsp_time.h"
#include "bsp_emergency_stop.h"
#include "bsp_debug_uart.h"
#include "DRV.h"
#include "adc_sample.h"
#include "PositionSensor.h"
#include "foc.h"                
#include "motor_config.h"   
#include "bsp_can.h"       
#include "CAN_com.h"
#include "app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
ControllerStruct controller;                       //保存FOC控制器运行状态
ObserverStruct observer;                           //保存电机温度和相电阻观测状态
MotorParameters motor_parameters;                  //保存电机参数
PositionSnapshot position_sample;                  //保存编码器采样结果
FocCommand command = {0};                          //保存控制命令，启动时目标值、增益和前馈扭矩全部为零
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void publish_command(const FocCommand *new_command); 
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void onMsgReceived(const CAN_RxFrame *rxMsg)       //在主循环中处理收到的CAN帧
{
	  MotorFeedback feedback;                         //保存本次回复的位置、速度和转矩
    CAN_TxHeaderTypeDef txHeader = {0};             //保存本次反馈帧的发送头
    uint8_t txMsg[8] = {0};                         //提供HAL读取的8字节缓冲，实际发送前6字节
    uint32_t txMailbox;                             //接收HAL选中的发送邮箱编号
    uint32_t primask;                               //保存读取反馈前的中断屏蔽状态
    float i_q_filt;                                 //暂存同一次读取的滤波q轴电流，A
    if ((rxMsg->header.IDE != CAN_ID_STD)
        || (rxMsg->header.RTR != CAN_RTR_DATA)
        || (rxMsg->header.DLC != 8U)
        || (rxMsg->header.StdId != (uint32_t)motor_parameters.CAN_ID))
    {
        return;                                         //仅解析发送给本机的标准8字节数据帧
    }
		
		controller.timeout = 0U;           //本机帧通过现有帧头检查后，清零CAN超时计数

    switch (unpack_special_cmd(rxMsg->data))             //识别原工程已有的特殊帧
    {
        case 0xFCU:
            can_mode_request = MOTOR_MODE;              //请求进入原工程的电机模式
            break;

        case 0xFDU:
            can_mode_request = REST_MODE;               //请求退出电机模式
            break;

        case 0xFEU:
            can_zero_request = 1;                       //沿用原工程的置零请求
            break;

        case 0xFAU:
            if (rxMsg->data[6] <= 127U)                 //保留原工程本机CAN ID的0～127范围
            {
                can_id_request = rxMsg->data[6];        //保存待设置的本机CAN ID
            }
            break;

        case 0x01U:
            can_master_request = rxMsg->data[6];        //保存待设置的主站ID，字节本身范围为0～255
            break;

        default:
            if (state == MOTOR_MODE)                       //保留原工程仅在电机模式下接收普通控制命令的条件
            {
                FocCommand new_command;                    //保存本次完整解码的五个命令字段

                unpack_cmd(rxMsg->data, &new_command);     //调用已有纯解码函数，还原位置、速度、增益和前馈转矩
                publish_command(&new_command);             //通过已有短临界区一次性发布完整命令
            }
            break;
    }
		    primask = __get_PRIMASK();                      //保存进入临界区前的中断屏蔽状态
    __disable_irq();                               //读取期间暂停控制中断，保证三个反馈量一致
    __DMB();                                       //保证反馈读取位于临界区内

    feedback.p = position_sample.theta_mech;        //读取输出轴机械位置，rad
    feedback.v = position_sample.dtheta_mech;       //读取输出轴机械速度，rad/s
    i_q_filt = controller.i_q_filt;                 //读取滤波后的q轴电流，A

    __DMB();                                       //保证反馈读取完成后再恢复中断
    __set_PRIMASK(primask);                         //恢复进入前的中断屏蔽状态

    feedback.t = i_q_filt * KT_OUT;                 //沿用原工程计算输出端估算转矩，N·m

    pack_reply(txMsg, (uint8_t)motor_parameters.CAN_ID,&feedback);                         //使用已有函数打包前6字节反馈数据

    txHeader.StdId = (uint32_t)motor_parameters.CAN_MASTER; //反馈帧发送到当前主站ID
    txHeader.IDE = CAN_ID_STD;                      //沿用标准帧
    txHeader.RTR = CAN_RTR_DATA;                    //发送数据帧
    txHeader.DLC = 6U;                              //反馈数据长度固定为6字节

    HAL_CAN_AddTxMessage(&hcan1,&txHeader,txMsg,&txMailbox);         //只提交一次，邮箱满或提交失败时放弃本次反馈
}


static void publish_command(const FocCommand *new_command) //发布一条完整的FOC控制命令
{
    uint32_t primask = __get_PRIMASK();                    //保存进入前的中断屏蔽状态

    __disable_irq();                                      //复制期间暂停可屏蔽中断
    __DMB();                                              //保证后续命令复制位于临界区内

    command = *new_command;                               //完整复制五个命令字段

    __DMB();                                              //保证命令写入排在恢复中断之前
    __set_PRIMASK(primask);                                //恢复进入前的中断屏蔽状态
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	CAN_RxFrame rxMsg;                                      //保存主循环本次从接收邮箱取出的完整帧
  BSP_SafeGpioEarlyInit();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_SPI3_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
	motor_parameters.CAN_ID = 1;                          //当前采用原工程无有效保存参数时的默认电机ID

  if (BSP_CAN_SetFilter((uint16_t)motor_parameters.CAN_ID) != HAL_OK) //启动CAN前配置本机ID过滤器
  {
    Error_Handler();                                  //配置失败时沿用工程现有错误处理
  }

  if (HAL_CAN_Start(&hcan1) != HAL_OK)                    //过滤器配置完成后启动CAN1
  {
    Error_Handler();                                  //启动失败时沿用工程现有错误处理
  }
	
	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) //首次开启FIFO0接收通知
  {
      Error_Handler();                                                       //沿用工程现有错误处理
  }
	
  if (!BSP_Time_Init())
  {
     Error_Handler();
  }
	
	if (DRV_Init() != HAL_OK)
  {
    Error_Handler();
  }
	reset_foc(&controller);                            //启动时复位原有FOC软件状态
	controller.v_bus = V_NOMINAL;                      //沿用原工程48V的母线电压滤波初值
  reset_observer(&observer);                         //启动时设置观测器温度和相电阻初值
	motor_parameters.I_MAX = I_MAX_MOTOR;              //使用原工程默认最大电流，单位A
	init_controller_params(&controller);               //启动控制中断前设置电流环增益
	
	Init_ADC();
	
  if (!zero_current(&controller.adc1_offset,&controller.adc2_offset))
  {
    Error_Handler();
  }
	

 PositionSensor_WriteLUT(motor_parameters.ENCODER_LUT);  	// 在启动周期采样前，将参数中的编码器校正表写入模块
	
	if (PositionSensor_ReadRaw(ENC1_CS_N_GPIO_Port,
                           ENC1_CS_N_Pin,
                           &position_sample.raw) != HAL_OK)    //预先发送主编码器角度命令
{
    Error_Handler();                                          //读取事务失败时沿用现有错误处理
}

if (PositionSensor_ReadRaw(ENC2_CS_N_GPIO_Port,
                           ENC2_CS_N_Pin,
                           &position_sample.raw2) != HAL_OK)   //预先发送副编码器角度命令
{
    Error_Handler();                                          //读取事务失败时沿用现有错误处理
} 
	
  __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);

  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

	if (!BSP_DebugUart_StartReceive())                 //启动USART2单字节中断接收
  {
    Error_Handler();                              //启动失败时沿用现有错误处理
  }

BSP_DebugUart_TryWrite("BOOT OK ERR=0\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		if (BSP_CAN_Read(&rxMsg) != 0U)                          //从现有接收邮箱取出一帧
    {
       onMsgReceived(&rxMsg);                              //在主循环中识别命令并投递App请求
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
	  BSP_EmergencyStop(BSP_ERROR_HAL);
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
