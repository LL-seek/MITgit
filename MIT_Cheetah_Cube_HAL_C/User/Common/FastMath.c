#include "FastMath.h"
#include "LUT.h"

static const float Multiplier = 81.4873308631f;             //角度转查找表索引的比例，等于512/(2π)

float FastSin(float theta)                                  //通过查找表计算theta的正弦值，theta单位为rad
{
    while (theta < 0.0f)                                    //角度小于0时，将角度调整到正数范围
    {
        theta += 6.28318530718f;                            //每次增加一整周，即2π
    }

    while (theta >= 6.28318530718f)                         //角度大于或等于一整周时进行回绕
    {
        theta -= 6.28318530718f;                            //每次减去一整周，即2π
    }

    return SinTable[(int)(Multiplier * theta)];             //将角度转换为索引并返回对应的正弦值
}

float FastCos(float theta)                                  //通过正弦相移计算theta的余弦值
{
    return FastSin(1.57079632679f - theta);                  //cos(theta)等于sin(π/2-theta)
}





