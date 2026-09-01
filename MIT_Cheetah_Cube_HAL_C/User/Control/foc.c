#include "foc.h"
#include <math.h>
#include "FastMath.h"
#include "hw_config.h"
#include "math_ops.h"


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













