#include "PreferenceWriter.h"
#include "motor_config.h"       
#include <math.h>               

void PreferenceWriter_Decode(MotorParameters *parameters,const int32_t int_reg[256],const float float_reg[64])
{
    uint32_t i;                                       //编码器校正表下标

    parameters->E_OFFSET  = float_reg[0];             //电角度偏置，rad
    parameters->M_OFFSET  = float_reg[1];             //机械零位偏置，rad
    parameters->I_BW      = float_reg[2];             //电流环带宽，Hz
    parameters->I_MAX     = float_reg[3];             //最大电流，A
    parameters->THETA_MIN = float_reg[4];             //保留原最小位置参数，未用于控制
    parameters->THETA_MAX = float_reg[5];             //保留原最大位置参数，未用于控制
    parameters->I_FW_MAX  = float_reg[6];             //最大弱磁电流，A

    parameters->PHASE_ORDER = int_reg[0];             //相序标志
    parameters->CAN_ID      = int_reg[1];             //本机CAN节点ID
    parameters->CAN_MASTER  = int_reg[2];             //CAN主站ID
    parameters->CAN_TIMEOUT = int_reg[3];             //超时阈值，单位为25微秒控制周期

    parameters->reserved_int[0] = int_reg[4];         //保留旧整数下标4的数据
    parameters->reserved_int[1] = int_reg[5];         //保留旧整数下标5的数据

    parameters->RAW_OFFSET  = int_reg[6];             //主编码器原始零位计数
    parameters->RAW2_OFFSET = int_reg[7];             //副编码器原始零位计数

    for (i = 0U; i < 128U; i++)                      //复制原工程的128项编码器校正表
    {
        parameters->ENCODER_LUT[i] = int_reg[8U + i]; //旧整数下标8～135对应校正表
    }
}


void PreferenceWriter_Validate(MotorParameters *parameters)   //启动参数处理
{
    if (isnan(parameters->E_OFFSET))
    {
        parameters->E_OFFSET = 0.0f;                         //电角度偏置为NaN时恢复为0
    }

    if (isnan(parameters->M_OFFSET))
    {
        parameters->M_OFFSET = 0.0f;                         //机械零位偏置为NaN时恢复为0
    }

    if (isnan(parameters->I_BW)
        || parameters->I_BW == -1.0f
        || parameters->I_BW == 0.0f)
    {
        parameters->I_BW = 1000.0f;                          //默认带宽，Hz
    }

    if (isnan(parameters->I_MAX)
        || parameters->I_MAX == -1.0f
        || parameters->I_MAX == 0.0f)
    {
        parameters->I_MAX = I_MAX_MOTOR;                     //默认最大电流，A
    }

    parameters->I_MAX =
        fmaxf(fminf(parameters->I_MAX, I_MAX_MOTOR), 0.0f);   //限制在0到电机最大电流之间

    if (isnan(parameters->I_FW_MAX)
        || parameters->I_FW_MAX == -1.0f)
    {
        parameters->I_FW_MAX = 0.0f;                         //默认关闭弱磁的处理
    }

    parameters->I_FW_MAX =
        fmaxf(fminf(parameters->I_FW_MAX, I_MAX_MOTOR), 0.0f); //限制在0到电机最大电流之间

    if (parameters->CAN_ID == -1)
    {
        parameters->CAN_ID = 1;                              //默认本机ID
    }

    if (parameters->CAN_MASTER == -1)
    {
        parameters->CAN_MASTER = 0;                          //默认主站ID
    }

    if (parameters->CAN_TIMEOUT == -1)
    {
        parameters->CAN_TIMEOUT = 0;                         //默认关闭超时的处理
    }
}



void PreferenceWriter_Load(MotorParameters *parameters)    //从Flash加载原工程参数
{
    const int32_t *int_reg = (const int32_t *)0x08040000U;  //旧整数参数区起始地址
    const float *float_reg = (const float *)0x08040400U;    //旧浮点参数区起始地址

    PreferenceWriter_Decode(parameters, int_reg, float_reg); //按已有索引映射填入参数结构体
}


HAL_StatusTypeDef PreferenceWriter_Flush(const MotorParameters *parameters)
{
    const int32_t int_reg[8] =
    {
        parameters->PHASE_ORDER,                       //对应旧整数下标0
        parameters->CAN_ID,                            //对应旧整数下标1
        parameters->CAN_MASTER,                        //对应旧整数下标2
        parameters->CAN_TIMEOUT,                       //对应旧整数下标3
        parameters->reserved_int[0],                   //保留旧整数下标4
        parameters->reserved_int[1],                   //保留旧整数下标5
        parameters->RAW_OFFSET,                        //对应旧整数下标6
        parameters->RAW2_OFFSET                        //对应旧整数下标7
    };

    const float float_reg[7] =
    {
        parameters->E_OFFSET,                          //对应旧浮点下标0
        parameters->M_OFFSET,                          //对应旧浮点下标1
        parameters->I_BW,                              //对应旧浮点下标2
        parameters->I_MAX,                             //对应旧浮点下标3
        parameters->THETA_MIN,                         //对应旧浮点下标4
        parameters->THETA_MAX,                         //对应旧浮点下标5
        parameters->I_FW_MAX                           //对应旧浮点下标6
    };

    union
    {
        float f;                                       //保存浮点参数值
        uint32_t u;                                    //读取同一数据的32位原始位模式
    } data;

    HAL_StatusTypeDef status;                          //保存HAL写入结果
    uint32_t i;                                        //参数写入下标

    for (i = 0U; i < 8U; i++)
    {
        status = FlashWriter_Write(ADDR_FLASH_SECTOR_6 + 4U * i,(uint32_t)int_reg[i]);//旧整数参数下标0～7的地址

        if (status != HAL_OK)
        {
            return status;                             //写入失败时结束本次写入
        }
    }

    for (i = 0U; i < 128U; i++)
    {
        status = FlashWriter_Write(ADDR_FLASH_SECTOR_6 + 4U * (8U + i),(uint32_t)parameters->ENCODER_LUT[i]);//旧LUT位于整数下标8～135

        if (status != HAL_OK)
        {
            return status;                             //将失败结果交给调用处处理
        }
    }

    for (i = 0U; i < 7U; i++)
    {
        data.f = float_reg[i];                         //保留浮点数原始位模式，不做数值转整数

        status = FlashWriter_Write(ADDR_FLASH_SECTOR_6 + 4U * (256U + i),data.u);//旧浮点参数区从0x08040400开始

        if (status != HAL_OK)
        {
            return status;                             //将失败结果交给调用处处理
        }
    }

    return HAL_OK;                                     //当前参数全部写入完成
}






