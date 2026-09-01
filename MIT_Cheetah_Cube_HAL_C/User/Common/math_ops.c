#include "math_ops.h"

#include <math.h>




float fmaxf3(float x, float y, float z)                         //返回三个浮点数中的最大值
{
    return fmaxf(fmaxf(x, y), z);                               //先比较x和y，再将较大值与z比较
}



float fminf3(float x, float y, float z)                         //返回三个浮点数中的最小值
{
    return fminf(fminf(x, y), z);                               //先比较x和y，再将较小值与z比较
}


void limit(float *x, float min, float max)                      //将*x限制在[min, max]范围内
{
    *x = fmaxf(fminf(*x, max), min);                            //先限制最大值，再限制最小值
}



void limit_norm(float *x, float *y, float limit)                  //限制二维向量(x, y)的最大模长
{
    float norm = sqrtf((*x) * (*x) + (*y) * (*y));                //计算向量模长sqrt(x2+y2)

    if (norm > limit)                                             //只有向量模长超过限制值时才进行缩放
    {
        float scale = limit / norm;                               //计算缩放比例，使新模长等于limit

        *x *= scale;                                              //按相同比例缩小x分量
        *y *= scale;                                              //按相同比例缩小y分量
    }
}



uint32_t float_to_uint(float x,float x_min,float x_max,uint8_t bits)                              //将浮点数从[x_min,x_max]映射到[0,2^bits-1]
{
    float span = x_max - x_min;                                   //计算浮点物理量的完整范围
    uint32_t max_int = (1UL << bits) - 1UL;                       //计算指定位数能够表示的最大无符号整数

    return (uint32_t)((x - x_min) * (float)max_int / span);       //移除最小值偏置，按比例映射并转换为整数
}


float uint_to_float(uint32_t x_int,float x_min,float x_max,uint8_t bits)                                 //将整数从[0,2^bits-1]还原到[x_min,x_max]
{
    float span = x_max - x_min;                                   //计算浮点物理量的完整范围
    uint32_t max_int = (1UL << bits) - 1UL;                       //计算指定位数能够表示的最大无符号整数

    return ((float)x_int * span / (float)max_int) + x_min;        //按比例还原浮点数，并加回最小值偏置
}



float normalize_angle_0_2pi(float angle_rad)                      //将输入角度归一化到[0, 2π)，单位rad
{
    const float two_pi = 2.0f * PI;                               //计算一整圈对应的角度2π
    float normalized = fmodf(angle_rad, two_pi);                  //计算angle_rad除以2π后的浮点余数

    if (normalized < 0.0f)                                        //如果余数为负数，则移动到[0, 2π)范围
    {
        normalized += two_pi;                                     //负角度加上一整圈
    }

    if ((normalized >= two_pi) || (normalized == 0.0f))           //处理2π边界，并统一正零和负零
    {
        normalized = 0.0f;                                        //将2π、+0和-0统一表示为+0
    }

    return normalized;                                            //返回[0, 2π)范围内的角度
}





















