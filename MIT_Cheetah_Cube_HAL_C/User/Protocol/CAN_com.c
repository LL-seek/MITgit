#include "CAN_com.h"

#include <math.h>

#include "math_ops.h"                            

#define P_MIN   (-12.5f)                          //目标位置最小值
#define P_MAX   (12.5f)                           //目标位置最大值

#define V_MIN   (-10.0f)                          //目标速度最小值
#define V_MAX   (10.0f)                           //目标速度最大值

#define KP_MIN  (0.0f)                            //位置增益最小值
#define KP_MAX  (500.0f)                          //位置增益最大值

#define KD_MIN  (0.0f)                            //速度增益最小值
#define KD_MAX  (5.0f)                            //速度增益最大值

#define T_MIN   (-36.0f)                          //前馈转矩最小值
#define T_MAX   (36.0f)                           //前馈转矩最大值

void unpack_cmd(const uint8_t data[8], FocCommand *command)  //将8字节MIT控制帧解码为FOC控制命令
{
    uint32_t p_int;                               //16位目标位置量化值
    uint32_t v_int;                               //12位目标速度量化值
    uint32_t kp_int;                              //12位位置增益量化值
    uint32_t kd_int;                              //12位速度增益量化值
    uint32_t t_int;                               //12位前馈转矩量化值

    p_int = ((uint32_t)data[0] << 8) |  (uint32_t)data[1];                   //组合data[0]和data[1]得到16位位置数据

    v_int = ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);             //组合data[2]和data[3]高4位得到12位速度数据

    kp_int = (((uint32_t)data[3] & 0x0FU) << 8) |   (uint32_t)data[4];       //组合data[3]低4位和data[4]得到12位Kp数据

    kd_int = ((uint32_t)data[5] << 4)  | ((uint32_t)data[6] >> 4);           //组合data[5]和data[6]高4位得到12位Kd数据

    t_int = (((uint32_t)data[6] & 0x0FU) << 8)  |   (uint32_t)data[7];       //组合data[6]低4位和data[7]得到12位转矩数据

    command->p_des = uint_to_float(p_int, P_MIN, P_MAX, 16U);    //将位置量化值还原为目标位置
    command->v_des = uint_to_float(v_int, V_MIN, V_MAX, 12U);    //将速度量化值还原为目标速度
    command->kp    = uint_to_float(kp_int, KP_MIN, KP_MAX, 12U); //将Kp量化值还原为位置增益
    command->kd    = uint_to_float(kd_int, KD_MIN, KD_MAX, 12U); //将Kd量化值还原为速度增益
    command->t_ff  = uint_to_float(t_int, T_MIN, T_MAX, 12U);    //将转矩量化值还原为前馈转矩
}


void pack_reply(uint8_t data[6],uint8_t can_id,const MotorFeedback *feedback)                 //将电机反馈编码为6字节MIT反馈帧
{
    float p;                                                   //限幅后的位置反馈
    float v;                                                   //限幅后的速度反馈
    float t;                                                   //限幅后的转矩反馈

    uint32_t p_int;                                            //16位位置量化值
    uint32_t v_int;                                            //12位速度量化值
    uint32_t t_int;                                            //12位转矩量化值

    p = fmaxf(fminf(feedback->p, P_MAX), P_MIN);               //将位置限制在协议范围内
    v = fmaxf(fminf(feedback->v, V_MAX), V_MIN);               //将速度限制在协议范围内
    t = fmaxf(fminf(feedback->t, T_MAX), T_MIN);               //将转矩限制在协议范围内

    p_int = float_to_uint(p, P_MIN, P_MAX, 16U);               //将位置转换为16位无符号整数
    v_int = float_to_uint(v, V_MIN, V_MAX, 12U);               //将速度转换为12位无符号整数
    t_int = float_to_uint(t, T_MIN, T_MAX, 12U);               //将转矩转换为12位无符号整数

    data[0] = can_id;                                          //第1字节存放本机CAN ID

    data[1] = (uint8_t)(p_int >> 8);                           //第2字节存放位置高8位
    data[2] = (uint8_t)(p_int & 0xFFU);                        //第3字节存放位置低8位

    data[3] = (uint8_t)(v_int >> 4);                           //第4字节存放速度高8位

    data[4] = (uint8_t)(((v_int & 0x0FU) << 4)
                      |  (t_int >> 8));                        //高4位存放速度低4位，低4位存放转矩高4位

    data[5] = (uint8_t)(t_int & 0xFFU);                        //第6字节存放转矩低8位
}



uint8_t unpack_special_cmd(const uint8_t data[8])     //识别原工程已有的五种特殊帧
{
    uint32_t i;                                       //前六个公共前导字节的下标

    for (i = 0U; i < 6U; i++)                         //五种特殊帧的前六字节都必须为0xFF
    {
        if (data[i] != 0xFFU)
        {
            return 0U;                                //不符合特殊帧格式
        }
    }

    if ((data[7] == 0xFAU) || (data[7] == 0x01U))     //修改本机ID或主站ID，第七字节存放新ID
    { 
        return data[7];                               //返回命令类型，新ID仍从data[6]取得
    }

    if ((data[6] == 0xFFU) && ((data[7] == 0xFCU) || (data[7] == 0xFDU) || (data[7] == 0xFEU)))  //进入、退出和置零要求前七字节全部为0xFF
    {
        return data[7];                               //返回进入、退出或置零命令
    }

    return 0U;                                        
}









