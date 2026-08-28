/*
管理GPIO口的安全
*/

#include "bsp_safe_gpio.h"
#include "main.h"



void BSP_SafeGpioEarlyInit(void)
{
	GPIO_InitTypeDef gpio_init = {0};     //初始化GPIO配置结构体变量
	
	__HAL_RCC_GPIOA_CLK_ENABLE();         //开启GPIOA时钟
	
	HAL_GPIO_WritePin(GPIOA, DRV_CS_N_Pin | ENC1_CS_N_Pin, GPIO_PIN_SET);     //PA4和PA15配置高电平
	
	HAL_GPIO_WritePin(GPIOA,PWM_DRV_A_Pin |PWM_DRV_B_Pin |PWM_DRV_C_Pin |DRV_ENABLE_Pin,GPIO_PIN_RESET);      // PA8 9 10 11配置低电平
	
	gpio_init.Pin = DRV_ENABLE_Pin;       //配置PA11的模式
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_PULLDOWN;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DRV_ENABLE_GPIO_Port, &gpio_init);
	
	gpio_init.Pin =DRV_CS_N_Pin |PWM_DRV_A_Pin |PWM_DRV_B_Pin |PWM_DRV_C_Pin |ENC1_CS_N_Pin;     // PA8 9 10 11配置模式
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio_init);
	
	__HAL_RCC_GPIOD_CLK_ENABLE();        //开启GPIOD时钟
	
	HAL_GPIO_WritePin(ENC2_CS_N_GPIO_Port,ENC2_CS_N_Pin, GPIO_PIN_SET);      //PA4和PA15配置模式
  gpio_init.Pin = ENC2_CS_N_Pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ENC2_CS_N_GPIO_Port, &gpio_init);
}	


