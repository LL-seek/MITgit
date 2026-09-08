#include "adc_sample.h"                                            
#include "adc.h"                                                   
#include "bsp_time.h"                                             


void Init_ADC(void)                                                //使能三路ADC并等待ADC稳定
{
    uint32_t start_cycles;                                         //记录ADC稳定等待的起始周期数

    __HAL_ADC_ENABLE(&hadc2);                                      //使能从ADC2
    __HAL_ADC_ENABLE(&hadc3);                                      //使能从ADC3
    __HAL_ADC_ENABLE(&hadc1);                                      //使能主ADC1

    start_cycles = BSP_Time_NowCycles32();                          //记录三路ADC使能完成时的DWT周期数
	  start_cycles = BSP_Time_NowCycles32();                          //从三路ADC使能完成后开始计时

    while (BSP_Time_ElapsedCycles32(start_cycles) <
           (SystemCoreClock / 1000000U) * ADC_STAB_DELAY_US)
    {
        /* 等待ADC上电稳定，仅在初始化阶段执行 */
    }

}



void ADC_Sample(AdcSnapshot *sample)                              //触发三路ADC同步转换并更新采样快照
{
    uint32_t start_cycles;                                        //记录本次ADC转换开始时的CPU周期数
    uint32_t timeout_cycles;                                      //保存5微秒对应的CPU周期数
	  sample->seq++;                                                //记录本次采样周期，超时也计入
    sample->valid = false;                                        //本轮采样开始时先清除有效标志

    timeout_cycles = (SystemCoreClock / 1000000U) * 5U;           //计算本次ADC转换允许等待的最大周期数

    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_STRT);   //清除ADC1上一轮的转换开始和完成标志
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_EOC | ADC_FLAG_STRT);   //清除ADC2上一轮的转换开始和完成标志
    __HAL_ADC_CLEAR_FLAG(&hadc3, ADC_FLAG_EOC | ADC_FLAG_STRT);   //清除ADC3上一轮的转换开始和完成标志

    start_cycles = BSP_Time_NowCycles32();                        //记录本次ADC转换开始前的DWT周期数

    ADC1->CR2 |= ADC_CR2_SWSTART;                                 //由主ADC1触发三路规则同步转换
  	while ((__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC) == RESET) ||
           (__HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_EOC) == RESET) ||
           (__HAL_ADC_GET_FLAG(&hadc3, ADC_FLAG_EOC) == RESET))
    {
      if (BSP_Time_ElapsedCycles32(start_cycles) >= timeout_cycles)
    {
        return;
    }
}



    sample->adc1_raw = (uint16_t)ADC1->DR;                         //读取ADC1电流通道的原始转换结果
    sample->adc2_raw = (uint16_t)ADC2->DR;                         //读取ADC2电流通道的原始转换结果
    sample->adc3_raw = (uint16_t)ADC3->DR;                         //读取ADC3母线电压通道的原始转换结果

    sample->valid = true;                                         //三路结果读取完成后将本次采样标记为有效
}



bool zero_current(int32_t *offset_1, int32_t *offset_2)        //采集两路电流传感器零偏并计算平均值
{
    int32_t adc1_offset = 0;                                   //ADC1电流通道原始值累加结果
    int32_t adc2_offset = 0;                                   //ADC2电流通道原始值累加结果
    const int32_t n = 50;                                      //零偏采样次数
    int32_t i;                                                 //零偏采样循环计数
    AdcSnapshot sample = {0};                                  //保存本次三路ADC采样快照

    for (i = 0; i < n; i++)                                    //连续采集50次ADC数据
    {
        ADC_Sample(&sample);                                   //同步采集三路ADC原始值
			  if (!sample.valid)
        {
         return false;
         }

        adc1_offset += sample.adc1_raw;                         //累加ADC1电流通道原始值
        adc2_offset += sample.adc2_raw;                         //累加ADC2电流通道原始值

        HAL_Delay(10U);                                        //保持原工程每次采样约10ms的间隔
    }

    *offset_1 = adc1_offset / n;                                //计算ADC1电流通道零偏平均值
    *offset_2 = adc2_offset / n;                                //计算ADC2电流通道零偏平均值

    return true;                                               
}

