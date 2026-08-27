
#include "../math_ops.h"


float fmaxf(float x, float y){
    return (((x)>(y))?(x):(y));
    }

float fminf(float x, float y){
    return (((x)<(y))?(x):(y));
    }

float fmaxf3(float x, float y, float z){
    return (x > y ? (x > z ? x : z) : (y > z ? y : z));
    }

float fminf3(float x, float y, float z){
    return (x < y ? (x < z ? x : z) : (y < z ? y : z));
    }

float roundf(float x){  
    return x < 0.0f ? ceilf(x - 0.5f) : floorf(x + 0.5f);
    }
    
void limit_norm(float *x, float *y, float limit){
    float norm = sqrt(*x * *x + *y * *y);
    if(norm > limit){
        *x = *x * limit/norm;
        *y = *y * limit/norm;
        }
    }
    
void limit(float *x, float min, float max){
    *x = fmaxf(fminf(*x, max), min);
    }


int float_to_uint(float x, float x_min, float x_max, int bits)            //将指定范围内的浮点物理量转换为指定位数的无符号编码值
{
    float span = x_max - x_min;                                           //计算物理量的完整范围
    float offset = x_min;                                                 //保存物理量的最小值，作为映射起点

    // 先用 x - offset 将物理量转换为从0开始的范围
    // 再乘以 (2^bits - 1)，映射到对应位数能够表示的整数范围
    // 最后除以物理量范围，并转换为整数，舍弃小数部分
    return (int)((x - offset) * ((float)((1 << bits) - 1)) / span);
}
    
    
float uint_to_float(int x_int, float x_min, float x_max, int bits){       //将无符号编码值还原成浮点数
    float span = x_max - x_min;                                           //计算完整物理范围
    float offset = x_min;                                                 //保存物理量下限
    return ((float)x_int)*span/((float)((1<<bits)-1)) + offset;           //映射物理范围
    }

		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		