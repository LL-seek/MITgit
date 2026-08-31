#ifndef MATH_OPS_H
#define MATH_OPS_H

#include <stdint.h>                                                

#define PI      3.14159265359f                                     //圆周率
#define SQRT3   1.73205080757f                                     //3的平方根

float fmaxf3(float x, float y, float z);                            //返回三个浮点数中的最大值
float fminf3(float x, float y, float z);                            //返回三个浮点数中的最小值

void limit_norm(float *x, float *y, float limit);                   //限制二维向量的最大模长
void limit(float *x, float min, float max);                         //将浮点数限制在[min, max]范围内

uint32_t float_to_uint(float x, float x_min, float x_max,uint8_t bits);       //将浮点物理量映射为指定位数的无符号整数

float uint_to_float(uint32_t x_int, float x_min, float x_max,uint8_t bits);   //将无符号整数还原为对应的浮点物理量

float normalize_angle_0_2pi(float angle_rad);                       //将角度归一化到[0, 2π)，rad

#endif