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
#include "PreferenceWriter.h"    
#include "calibration.h"                              //接入原工程的相序判断和编码器标定
#include "pwm.h"   
#include <stdio.h>             
#include <stdlib.h>                                    
#include <math.h>                                      
#include <string.h>                                    
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
ControllerStruct controller;                           //保存FOC控制器运行状态
ObserverStruct observer;                               //保存观测器运行状态

MotorParameters motor_parameters =                     //保存电机参数，并设置启动初值
{
    .I_BW   = 1000.0f,                                  //电流环带宽默认值，Hz
    .I_MAX  = I_MAX_MOTOR,                               //最大电流默认值，A
    .CAN_ID = 1                                         //本机CAN节点默认ID
};

PositionSnapshot position_sample;                      //保存编码器采样结果
FocCommand command = {0};                               //保存控制命令，启动时全部为零

static char cmd_val[8] = {0};                           //沿用原工程的数值缓冲区
static char cmd_id = 0;                                 //沿用原工程的参数前缀
static char char_count = 0;                             //已接收字符数，包含一个参数前缀
static int setup_save_pending = 0;                      //设置命令保存请求，0无请求，1等待保存
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
    if ((rxMsg->header.IDE != CAN_ID_STD)|| (rxMsg->header.RTR != CAN_RTR_DATA)|| (rxMsg->header.DLC != 8U)|| (rxMsg->header.StdId != (uint32_t)motor_parameters.CAN_ID))
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

static void serial_interrupt(void)                      //在主循环中处理原工程的串口模式命令
{
    char c;                                             //保存本次取出的字符
                                             //保存本次取出的字符
int8_t result = BSP_DebugUart_Read(&c);                  //读取字符和现有接收状态

if (result <= 0)
{
    if (result < 0)                                    //已有接收接口报告输入丢失
    {
        char_count = 0;                                //放弃未收完整的命令
        cmd_id = 0;
        memset(cmd_val, 0, sizeof(cmd_val));
    }

    return;                                            //本次没有可处理的字符
}

    if (c == 27)                                        //沿用原工程，Esc在各模式下请求返回菜单
    {
        can_mode_request = REST_MODE;                   //记录退出当前模式的请求
        return;
    }

    if (state == REST_MODE)                             //沿用原工程，仅在停止模式下接受菜单命令
    {
        switch (c)
        {
            case 'c':
                can_mode_request = CALIBRATION_MODE;    //请求进入编码器标定模式
                break;

            case 'm':
                can_mode_request = MOTOR_MODE;          //请求进入电机运行模式
                break;

            case 'e':
                can_mode_request = ENCODER_MODE;        //请求进入编码器显示模式
                break;

            case 's':
                can_mode_request = SETUP_MODE;          //请求进入参数设置模式
                break;

            case 'z':
                can_zero_request = 1;                   //沿用原工程的置零请求
                break;
        }
    }
		else if (state == SETUP_MODE)                           //处理原工程的参数设置命令
{
    if (c == 13)                                       //沿用原工程，收到回车后解析参数
    {
        switch (cmd_id)
        {
            case 'b':
                motor_parameters.I_BW =
                    fmaxf(fminf((float)atof(cmd_val),
                                2000.0f), 100.0f);      //沿用原工程的100～2000Hz范围
                break;

            case 'i':
                motor_parameters.CAN_ID = atoi(cmd_val); //沿用原工程的本机ID转换
                break;

            case 'm':
                motor_parameters.CAN_MASTER = atoi(cmd_val); //设置模式下m表示主站ID
                break;

            case 'l':
                motor_parameters.I_MAX =
                    fmaxf(fminf((float)atof(cmd_val),
                                I_MAX_MOTOR), 0.0f);    //沿用原工程的最大电流限幅，A
                break;

            case 'f':
                motor_parameters.I_FW_MAX =
                    fmaxf(fminf((float)atof(cmd_val),
                                I_MAX_MOTOR), 0.0f);    //沿用原工程的弱磁电流限幅，A
                break;

            case 't':
                motor_parameters.CAN_TIMEOUT = atoi(cmd_val); //沿用原工程的超时周期数
                break;

            default:
                BSP_DebugUart_TryWrite(
                    "\r\nNot a valid command prefix\r\n"); //沿用原工程的未知前缀提示
                break;
        }

        setup_save_pending = 1;                        //承接原工程回车后的保存和菜单刷新流程
    }
    else
    {
        if (char_count == 0)
        {
            cmd_id = c;                                //第一个字符作为参数前缀
            char_count = 1;
        }
        else if (char_count < sizeof(cmd_val))
        {
            cmd_val[char_count - 1] = c;                //后续字符写入数值缓冲区
            cmd_val[char_count] = '\0';                 //始终保留字符串结束符
            char_count++;
        }
        else
        {
            cmd_id = 0;                                //本条输入过长，不使用截断后的数值修改参数
        }

        char echo[2] = {c, '\0'};                       //将当前字符组成字符串
        BSP_DebugUart_TryWrite(echo);                   //通过现有串口接口回显输入
    }
}
}

static void enter_setup_state(void)                     //执行原工程参数设置模式的入口动作
{
    char text[128];                                    //保存本次显示的六个参数及输入提示
	  char_count = 0;                                        //开始接收下一条完整命令
    cmd_id = 0;
    memset(cmd_val, 0, sizeof(cmd_val));                     //清除上一条命令的数值

    if (!zero_current(&controller.adc1_offset,
                      &controller.adc2_offset))         //保留原工程进入设置模式时的电流零偏采集
    {
        Error_Handler();                               //采集失败时沿用现有错误处理
    }

    snprintf(text, sizeof(text),                       //将当前参数组合成一次串口输出
             "\r\nConfiguration Options\r\n"
             "b=%.1f Hz i=%ld m=%ld\r\n"
             "l=%.1f A f=%.1f A t=%ld cycles\r\n"
             "prefix+value+Enter; Esc=menu\r\n",
             (double)motor_parameters.I_BW,            //电流环带宽，Hz
             (long)motor_parameters.CAN_ID,             //本机CAN ID
             (long)motor_parameters.CAN_MASTER,         //主站CAN ID
             (double)motor_parameters.I_MAX,            //最大电流，A
             (double)motor_parameters.I_FW_MAX,         //最大弱磁电流，A
             (long)motor_parameters.CAN_TIMEOUT);       //CAN超时阈值，单位为控制周期

    BSP_DebugUart_TryWrite(text);                       //通过现有非阻塞串口接口发送参数
}

static void print_encoder(void)
{
    static uint32_t last_print = 0U;                    //记录上次显示时间，单位 ms
    uint32_t now = HAL_GetTick();                       //读取当前毫秒计时
    float theta_mech;                                  //保存本次显示的输出轴机械角度，单位 rad
    char text[96];                                     //保存原工程的两行显示内容

    if ((uint32_t)(now - last_print) < 500U)             //保持原工程约 500 ms 的显示间隔
    {
        return;
    }

    last_print = now;
    theta_mech = position_sample.theta_mech;            //读取已有周期采样结果

    snprintf(text, sizeof(text),
             "Mechanical Angle_GR:%f\r\n"
             "theta_mech:%f\r\n",
             (double)theta_mech,
             (double)theta_mech);                       //原工程这两行对应同一个机械角度

    BSP_DebugUart_TryWrite(text);                       //通过现有串口接口输出
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
	
	PreferenceWriter_Load(&motor_parameters);      //先从Flash读取旧参数并转换到参数结构体
  PreferenceWriter_Validate(&motor_parameters);  //再按原工程逻辑处理默认值和电流限幅

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
		serial_interrupt();                                    //处理串口字符并记录请求
		if ((can_id_request >= 0)
    || (can_master_request >= 0)
    || setup_save_pending)                             //同时承接串口设置命令的保存请求
   {
    HAL_StatusTypeDef status;                          //保存解锁、擦除和写入结果
    HAL_StatusTypeDef close_status;                    //单独保存Flash上锁结果

    if (PWM_Stop() != HAL_OK)                          //进入COAST，停止三相PWM和TIM1更新中断
    {
        Error_Handler();                              //停止输出失败时，沿用现有错误处理
    }

    state = setup_save_pending ? SETUP_MODE : REST_MODE;    //串口设置继续留在SETUP，CAN设置沿用REST                                //保存期间退出电机模式
    can_mode_request = -1;                            //清除保存前遗留的模式切换请求
    command = (FocCommand){0};                         //清零原有控制命令
    reset_foc(&controller);                           //清除电流参考和控制器积分等运行状态

    if (can_id_request >= 0)                            //收到CAN修改ID请求时才使用该请求值
    {
        motor_parameters.CAN_ID = can_id_request;      //把待设置的本机ID写入RAM参数
    }

    if (can_master_request >= 0)
    {
        motor_parameters.CAN_MASTER = can_master_request; //把待设置的主站ID写入RAM参数
    }

    status = FlashWriter_Open();                      //解锁Flash并擦除Sector 6

    if (status == HAL_OK)
    {
        status = PreferenceWriter_Flush(&motor_parameters); //按旧布局写回全部已映射参数
    }

    close_status = FlashWriter_Close();               //无论擦写成功或失败，都执行上锁

    if ((status != HAL_OK) || (close_status != HAL_OK))
    {
        Error_Handler();                              //保存失败时保持停止，沿用现有错误处理
    }

    PreferenceWriter_Load(&motor_parameters);         //保存成功后，沿用原工程重新加载参数

    if ((can_id_request >= 0)
        || (setup_save_pending && cmd_id == 'i'))       //CAN或串口修改本机ID后，同步更新过滤器
    {
        if (BSP_CAN_SetFilter((uint16_t)motor_parameters.CAN_ID) != HAL_OK)
        {
            Error_Handler();                          //更新本机ID过滤器失败时，沿用现有错误处理
        }
    }

    can_id_request = -1;                              //清除已处理的本机ID请求，避免重复擦写
    can_master_request = -1;                          //清除已处理的主站ID请求，避免重复擦写
		
		if (setup_save_pending)
{
    setup_save_pending = 0;                            //清除已完成的保存请求
    enter_setup_state();                               //沿用原工程，保存后重新显示设置菜单
}

    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);     //恢复周期中断前清除更新标志

    if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)        //恢复周期中断，使采样和反馈继续更新
    {
        Error_Handler();                              //恢复失败时，沿用现有错误处理
    }
   }
	 if ((can_mode_request == REST_MODE)
       || (can_mode_request == CALIBRATION_MODE)        //接入现有串口c命令的标定请求
       || (can_mode_request == MOTOR_MODE)
       || (can_mode_request == SETUP_MODE)
       || (can_mode_request == ENCODER_MODE))            //接入原工程的编码器显示模式
  {
    int next_state = can_mode_request;                   //取出本次请求的目标模式
    can_mode_request = -1;                              //清除已取出的请求

    if ((next_state != state) || (next_state == REST_MODE))
    {
        if (BSP_EmergencyStop_IsActive())                //沿用现有急停锁存，禁止重新启动已关断的定时器
        {
            Error_Handler();                            //进入工程现有错误处理
        }

        if (PWM_Stop() != HAL_OK)                        //先进入COAST，并停止PWM和TIM1更新中断
        {
            Error_Handler();                            //停止失败时沿用现有错误处理
        }

        state = REST_MODE;                              //切换准备期间保持停止模式
        command = (FocCommand){0};                      //清除上一次运行留下的控制命令
        reset_foc(&controller);                         //复位电流参考和控制器运行状态
        controller.timeout = 0U;                        //重新开始累计CAN超时周期

        if (next_state == MOTOR_MODE)
        {
            if ((PWM_Start() != HAL_OK)
                || BSP_EmergencyStop_IsActive())         //启动PWM，并承接启动期间已有的急停处理
            {
                Error_Handler();                        //启动失败时沿用现有错误处理
            }

            state = MOTOR_MODE;                         //启动成功后，允许控制中断执行FOC
        }
        else
    {
    state = next_state;                                //进入请求的非电机模式

    if (state == SETUP_MODE)
    {
        enter_setup_state();                           //TIM1采样中断停止期间执行设置入口
    }

    if (state == CALIBRATION_MODE)
    {
        HAL_StatusTypeDef status;
        HAL_StatusTypeDef close_status;

        if ((PWM_Start() != HAL_OK) || BSP_EmergencyStop_IsActive())
        {
            Error_Handler();                            //沿用现有PWM启动和急停处理
        }

        order_phases(&position_sample, &controller, &motor_parameters);
        if (BSP_EmergencyStop_IsActive())
        {
            Error_Handler();                            //承接现有采样中断触发的关断
        }

        calibrate_encoder(&position_sample, &motor_parameters); //在主循环顺序完成正反扫描、偏置和LUT计算
        if ((PWM_Stop() != HAL_OK) || BSP_EmergencyStop_IsActive())
        {
            Error_Handler();                            //保存前进入COAST并停止PWM和TIM1更新中断
        }
        reset_foc(&controller);

        status = FlashWriter_Open();                    //复用阶段8已有的参数保存接口
        if (status == HAL_OK)
        {
            status = PreferenceWriter_Flush(&motor_parameters);
        }
        close_status = FlashWriter_Close();             //保存后恢复Flash上锁
        if ((status != HAL_OK) || (close_status != HAL_OK))
        {
            Error_Handler();                            //保存失败时沿用现有错误处理
        }

        HAL_Delay(200U);                                 //保留原工程完成提示前的等待
        BSP_DebugUart_TryWrite("\r\nCalibration complete. Press 'esc' to return to menu\r\n");
    }

    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);       //恢复周期采样前清除更新标志

    if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)         //恢复采样，三相PWM继续保持停止
    {
        Error_Handler();                               //恢复失败时沿用现有错误处理
    }
    }
    }
   }

    if (state == ENCODER_MODE)                          //进入编码器模式后周期显示角度
    {
        print_encoder();
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
