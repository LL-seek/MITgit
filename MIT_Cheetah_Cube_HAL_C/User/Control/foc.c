#include "foc.h"
#include <math.h>
#include "FastMath.h"
#include "hw_config.h"
#include "math_ops.h"
#include "motor_config.h"      


void init_controller_params(ControllerStruct *controller)  //初始化原工程电流环增益
{
    controller->ki_d = KI_D;                               //设置d轴每周期积分系数
    controller->ki_q = KI_Q;                               //设置q轴每周期积分系数
    controller->k_d = K_D;                                 //设置d轴比例增益，V/A
    controller->k_q = K_Q;                                 //设置q轴比例增益，V/A
}

void reset_foc(ControllerStruct *controller)       //复位当前FOC运行状态
{
    controller->i_d_ref = 0.0f;                   //清零d轴电流参考值，A
    controller->i_q_ref = 0.0f;                   //清零q轴电流参考值，A

    controller->i_d = 0.0f;                       //清零d轴电流反馈值，A
    controller->i_q = 0.0f;                       //清零q轴电流反馈值，A
    controller->i_d_filt = 0.0f;                  //清零d轴电流滤波状态，A
    controller->i_q_filt = 0.0f;                  //清零q轴电流滤波状态，A

    controller->d_int = 0.0f;                     //清零d轴电流环积分电压，V
    controller->q_int = 0.0f;                     //清零q轴电流环积分电压，V
    controller->fw_int = 0.0f;                    //清零弱磁积分，A

    controller->v_d = 0.0f;                       //清零d轴输出电压，V
    controller->v_q = 0.0f;                       //清零q轴输出电压，V
    controller->v_ref = 0.0f;                     //清零供弱磁使用的历史电压幅值，V

    controller->v_u = 0.0f;                       //清零U相电压计算结果，V
    controller->v_v = 0.0f;                       //清零V相电压计算结果，V
    controller->v_w = 0.0f;                       //清零W相电压计算结果，V

    controller->dtc_u = DTC_SAFE;                 //将U相占空比缓存恢复为中点
    controller->dtc_v = DTC_SAFE;                 //将V相占空比缓存恢复为中点
    controller->dtc_w = DTC_SAFE;                 //将W相占空比缓存恢复为中点
}

void reset_observer(ObserverStruct *observer)       //按原工程恢复观测器初值
{
    observer->temperature = 25.0f;                 //将热模型估算温度恢复为25℃
    observer->resistance = R_PHASE;                //将估算相电阻恢复为电机配置值，Ω
}

bool FOC_SetAdcSnapshot(ControllerStruct *controller,const AdcSnapshot *sample,const MotorParameters *parameters)
{
    controller->adc.adc1_raw = sample->adc1_raw; //ADC1原始值
    controller->adc.adc2_raw = sample->adc2_raw; //ADC2原始值
    controller->adc.adc3_raw = sample->adc3_raw; //ADC3原始值
    controller->adc.seq      = sample->seq;      //复制本轮采样序号
    controller->adc.valid    = sample->valid;    //复制有效位，此时为 true
	
	  if (!controller->adc.valid)
    {
        return false;
    }
	
	  controller->v_bus = 0.95f * controller->v_bus                      //保留上一轮滤波后母线电压的95%
                              + 0.05f                                  //本轮采样换算得到的母线电压占5%
                              * (float)controller->adc.adc3_raw        //将ADC3母线电压原始码值转换为浮点数
                              * V_SCALE;                               //乘以电压换算系数，更新滤波后的母线电压

		if (parameters->PHASE_ORDER)                                       //相序标志非零：ADC2对应B相电流，ADC1对应C相电流
    {
       controller->i_b = I_SCALE                                               //使用电流换算系数
                                * (float)((int32_t)controller->adc.adc2_raw    //将ADC2原始值转为有符号整数，参与零偏相减
                                - controller->adc2_offset);                    //减去ADC2零偏，差值转为浮点数后换算为B相电流

       controller->i_c = I_SCALE                                               //使用电流换算系数
                                * (float)((int32_t)controller->adc.adc1_raw    //将ADC1原始值转为有符号整数，参与零偏相减
                                - controller->adc1_offset);                    //减去ADC1零偏，差值转为浮点数后换算为C相电流
    }
    else                                                                       //相序标志为0：ADC1对应B相电流，ADC2对应C相电流
    {
       controller->i_b = I_SCALE                                               //使用电流换算系数
                                * (float)((int32_t)controller->adc.adc1_raw    //将ADC1原始值转为有符号整数，参与零偏相减
                                - controller->adc1_offset);                    //减去ADC1零偏，差值转为浮点数后换算为B相电流

       controller->i_c = I_SCALE                                               //使用电流换算系数
                                * (float)((int32_t)controller->adc.adc2_raw    //将ADC2原始值转为有符号整数，参与零偏相减
                                - controller->adc2_offset);                    //减去ADC2零偏，差值转为浮点数后换算为C相电流
     }

    controller->i_a = -controller->i_b - controller->i_c;                      //依据三相电流之和为0，由B、C相电流计算A相电流													
															
    return true;                            
}  


void abc(float theta, float d, float q, float *a, float *b, float *c)  //逆Park和逆Clarke变换：dq坐标转换为三相abc坐标
{
    float cf = FastCos(theta);                                         //计算电角度theta的余弦值
    float sf = FastSin(theta);                                         //计算电角度theta的正弦值

    *a = cf * d - sf * q;                                              //计算a相分量

    *b = (0.86602540378f * sf - 0.5f * cf) * d - (-0.86602540378f * cf - 0.5f * sf) * q;                       //计算b相分量

    *c = (-0.86602540378f * sf - 0.5f * cf) * d - (0.86602540378f * cf - 0.5f * sf) * q;                       //计算c相分量
}

void dq0(float theta, float a, float b, float c, float *d, float *q)   //Clarke和Park变换：三相abc坐标转换为dq坐标
{
    float cf = FastCos(theta);                                         //计算电角度theta的余弦值
    float sf = FastSin(theta);                                         //计算电角度theta的正弦值

    *d = 0.6666667f * (cf * a + (0.86602540378f * sf - 0.5f * cf) * b + (-0.86602540378f * sf - 0.5f * cf) * c);                       //计算d轴分量

    *q = 0.6666667f * (-sf * a - (-0.86602540378f * cf - 0.5f * sf) * b - (0.86602540378f * cf - 0.5f * sf) * c);                      //计算q轴分量
}



void svm(float v_bus,float u,float v,float w,float *dtc_u,float *dtc_v,float *dtc_w)                                           //空间矢量调制：将三相电压转换为三相PWM占空比
{
    float v_offset;                                               //三相电压的共模偏置

    v_offset = (fminf3(u, v, w) + fmaxf3(u, v, w)) * 0.5f;        //计算最大和最小相电压的中点

    *dtc_u = fminf(fmaxf((u - v_offset) / v_bus + 0.5f, DTC_MIN),DTC_MAX);       //计算U相占空比并限制在允许范围内

    *dtc_v = fminf(fmaxf((v - v_offset) / v_bus + 0.5f, DTC_MIN),DTC_MAX);       //计算V相占空比并限制在允许范围内

    *dtc_w = fminf(fmaxf((w - v_offset) / v_bus + 0.5f, DTC_MIN),DTC_MAX);       //计算W相占空比并限制在允许范围内
}


void torque_control(ControllerStruct *controller,FocCommand *command,const PositionSnapshot *position)  //迁移原工程的位置、速度和扭矩外环
{
    float torque_ref;                                 //目标输出轴扭矩，N·m

    if (command->p_des >= PI)                         //目标位置达到或超过原工程上限
    {
        command->p_des = PI;                          //将目标位置限制为正π，rad
    }
    else if (command->p_des <= -PI)                   //目标位置达到或低于原工程下限
    {
        command->p_des = -PI;                         //将目标位置限制为负π，rad
    }

    torque_ref = command->kp * (command->p_des - position->theta_mech)       //位置误差产生的输出轴扭矩
                 + command->t_ff                                             //前馈输出轴扭矩
                 + command->kd * (command->v_des - position->dtheta_mech);   //速度误差产生的输出轴扭矩

    controller->i_q_ref = torque_ref / KT_OUT;         //由输出轴扭矩换算q轴电流参考值，A
    controller->i_d_ref = 0.0f;                        //沿用原工程外环给出的d轴电流参考值，A
}



void linearize_dtc(float *dtc)                      //沿用原工程的归一化电压补偿
{
    float sgn = 1.0f - 2.0f * (*dtc < 0.0f);       //根据电压分量确定符号，负值为-1，非负值为1

    if (fabsf(*dtc) >= 0.01f)                      //归一化电压幅值达到原工程的分段阈值
    {
        *dtc = *dtc * 0.986f + 0.014f * sgn;       //沿用原工程的大幅值补偿系数
    }
    else
    {
        *dtc = 2.5f * (*dtc);                      //沿用原工程的小幅值补偿，零输入仍为零
    }
}













