#include "PositionSensor.h"
#include "spi.h"
#include "bsp_time.h"
#include <string.h>
#include "math_ops.h"                 
#include "motor_config.h"           

static int32_t offset_lut[128] = {0};
static const int32_t _CPR = 16384;      //保持原工程每圈16384个计数
static int32_t rotations = 0;           //主编码器累计跨圈数
static int32_t old_counts = 8192;
static float modPosition = 0.0f;          //主编码器上一次采样的单圈机械角度，单位rad
static float velVec[40] = {0.0f};         //主编码器最近40次瞬时机械角速度，单位rad/s
static float ElecVelocityFilt = 0.0f;     //原工程内部递推的低通电角速度，单位rad/s
static bool DualEncoder = true;       //圈数初始化标志，true待初始化，false已完成

HAL_StatusTypeDef PositionSensor_ReadRaw(GPIO_TypeDef *cs_port, uint16_t cs_pin, uint16_t *raw)  //读取指定编码器的14位原始计数
{
    uint16_t readAngleCmd = 0x7FFEU;                                    //保原工程的角度读取命令
    uint16_t rx_data;                                                   //编码器返回的16位数据
    uint32_t start_cycles;                                              //本次SPI事务的起始周期
    uint32_t timeout_cycles;                                            //5微秒对应的CPU周期数

    timeout_cycles = (SystemCoreClock / 1000000U) * 5U;                 //本次SPI事务的等待上限为5微秒
    start_cycles = BSP_Time_NowCycles32();                              //本次SPI事务的起始周期

    __HAL_SPI_ENABLE(&hspi3);                                           //使能Cube配置好的SPI3

    while ((__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_TXE) == RESET) ||       //等待发送数据寄存器为空
           (__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_BSY) != RESET))         //等待上一笔SPI传输结束
    {
        if (BSP_Time_ElapsedCycles32(start_cycles) >= timeout_cycles)   //检查本次等待是否超过5微秒
        {
            return HAL_TIMEOUT;                                         //片选尚未拉低，直接返回超时
        }
    }

    __HAL_SPI_CLEAR_OVRFLAG(&hspi3);                                    //清除上次未读取的数据和溢出标志

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);                 //拉低片选并选中当前编码器

    *(__IO uint16_t *)&hspi3.Instance->DR = readAngleCmd;               //写入16位角度读取命令

    while ((__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_RXNE) == RESET) ||      //等待接收数据寄存器非空
           (__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_TXE) == RESET) ||       //等待发送数据寄存器为空
           (__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_BSY) != RESET))         //等待本次SPI传输结束
    {
        if (BSP_Time_ElapsedCycles32(start_cycles) >= timeout_cycles)   //检查整个SPI事务是否超过5微秒
        {
            HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);           //拉高片选并结束本次事务
            return HAL_TIMEOUT;                                        //返回超时且不覆盖原来的raw
        }
    }

    rx_data = *(__IO uint16_t *)&hspi3.Instance->DR;                    //读取编码器返回的16位数据
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);                   //拉高片选并结束本次SPI事务

    *raw = (uint16_t)(rx_data & 0x3FFFU);                               //保留编码器返回值的低14位
    return HAL_OK;                                                      //返回本次读取成功
}

void PositionSensor_WriteLUT(const int32_t new_lut[128])
{
    // 将128项校正数据复制到模块内部的LUT
    memcpy(offset_lut, new_lut, sizeof(offset_lut));
}

void PositionSensor_Sample(PositionSnapshot *sample,const MotorParameters *parameters,float dt)
{
    uint16_t raw;                                                             //主编码器原始角度计数
    uint16_t raw2;                                                            //副编码器原始角度计数
    int32_t off_1;                                                            //当前LUT节点的角度补偿量
    int32_t off_2;                                                            //下一个LUT节点的角度补偿量
    int32_t off_interp;                                                       //线性插值得到的角度补偿量
	  int32_t e1;                         //主编码器扣除原始零位偏置后的计数
    int32_t e2;                         //副编码器扣除原始零位偏置后的计数
    int32_t diff;                       //归一到正负半圈范围内的编码器差值
    float K;                            //原工程根据齿差估算的圈数，保留小数
	  float elec;                         //当前电角度，单位rad
	  float oldModPosition;                    //上一次采样的单圈机械角度，单位rad
    float vel;                               //本次瞬时机械角速度，单位rad/s
    float sum;                               //40次瞬时机械角速度的和，单位rad/s
    float MechVelocity;                      //40点平均后的电机轴机械角速度，单位rad/s
    float ElecVelocity;                      //电机电角速度，单位rad/s
    int n = 40;                              //保持原工程40点平均窗口
    int i;                                   //速度历史数组的移动索引


    sample->valid = false;                                                    //开始采样前将位置数据标记为无效

    if (PositionSensor_ReadRaw(ENC1_CS_N_GPIO_Port,                           //读取主编码器原始角度计数
                               ENC1_CS_N_Pin,
                               &raw) != HAL_OK)
    {
        return;                                                               //主编码器读取失败时结束本次采样
    }

    if (PositionSensor_ReadRaw(ENC2_CS_N_GPIO_Port,                           //读取副编码器原始角度计数
                               ENC2_CS_N_Pin,
                               &raw2) != HAL_OK)
    {
        return;                                                               //副编码器读取失败时结束本次采样
    }
		
	  e1 = (int32_t)raw - parameters->RAW_OFFSET;       //主编码器原始计数减去零位偏置
    e2 = (int32_t)raw2 - parameters->RAW2_OFFSET;     //副编码器原始计数减去零位偏置

    if (e1 < 0)
    {
        e1 += _CPR;                                 //负计数加一圈，与原工程一致
    }

    if (e2 < 0)
    {
        e2 += _CPR;                                 //负计数加一圈，与原工程一致
    }

    diff = e1 - e2;                                  //按主编码器减副编码器计算差值

    if (diff < -(_CPR / 2))
    {
        diff += _CPR;                               //差值小于负半圈时加一圈
    }

    if (diff > (_CPR / 2))
    {
        diff -= _CPR;                               //差值大于正半圈时减一圈
    }

    K = DUAL_ENCODER_DIR * (float)diff * GR
        / (float)_CPR;                              //保持原工程的齿差圈数计算

    sample->theta_mech = K * (2.0f * PI / GR);        //

    off_1 = offset_lut[raw >> 7];                                             //读取当前角度分段的LUT补偿量
    off_2 = offset_lut[((raw >> 7) + 1) % 128];                               //读取下一分段的LUT补偿量，末项连接第0项
    off_interp = off_1 +                                                      //在相邻两个LUT节点之间进行整数线性插值
              (((off_2 - off_1) *
                (raw - ((raw >> 7) << 7))) >> 7);

    sample->raw = raw;                                                        //保存主编码器原始角度计数
    sample->raw2 = raw2;                                                      //保存副编码器原始角度计数
    sample->angle = (int32_t)raw + off_interp;                                //保存主编码器经过LUT校正后的角度计数
		if (DualEncoder)
{
    rotations = (int32_t)K;                      //首次采样时按原工程向零截断，设置累计圈数初值
    DualEncoder = false;                        //完成初始化，后续采样继续累计跨圈
}
else if (sample->angle - old_counts > _CPR / 2)
{
    rotations -= 1;                             //计数由小值跨到大值，累计圈数减一
}
else if (sample->angle - old_counts < -_CPR / 2)
{
    rotations += 1;                             //计数由大值跨到小值，累计圈数加一
}

old_counts = sample->angle;                      //保存本次校正计数，供下一次跨圈判断

    sample->position =(2.0f * PI *((float)sample->angle + (_CPR * rotations))) / (float)_CPR;                            //将包含累计圈数的计数转换为机械角度

		elec = (2.0f * PI / (float)_CPR)* (float)((NPP * sample->angle) % _CPR)+ parameters->E_OFFSET;                       //校正计数乘极对数，换算为电角度并加上电偏置

    if (elec < 0.0f)
    {
        elec += 2.0f * PI;                             //电角度小于零时加一个电周期
    }
    else if (elec > 2.0f * PI)
    {
        elec -= 2.0f * PI;                             //电角度大于一个电周期时减去一个电周期
    }

    sample->theta_elec = elec;                                  //保存本次电角度，单位rad

oldModPosition = modPosition;                               //保存上一次主编码器单圈机械角度
modPosition = (2.0f * PI * (float)sample->angle)
              / (float)_CPR;                               //将本次LUT校正计数换算为单圈机械角度

if ((modPosition - oldModPosition) < -3.0f)                  //保持原工程的正向跨零判断
{
    vel = (modPosition - oldModPosition + 2.0f * PI) / dt;   //补回一圈后计算瞬时机械角速度
}
else if ((modPosition - oldModPosition) > 3.0f)              //保持原工程的反向跨零判断
{
    vel = (modPosition - oldModPosition - 2.0f * PI) / dt;   //减去一圈后计算瞬时机械角速度
}
else
{
    vel = (modPosition - oldModPosition) / dt;              //没有跨零时直接用角度差计算速度
}

sum = vel;                                                 //先计入本次瞬时机械角速度

for (i = 1; i < n; i++)                                     //保持原工程的固定长度数组移动
{
    velVec[n - i] = velVec[n - i - 1];                      //将上一周期的速度历史向后移动
    sum += velVec[n - i];                                  //累加保留下来的历史速度
}

velVec[0] = vel;                                            //保存本次最新的瞬时机械角速度
MechVelocity = sum / (float)n;                              //计算40点平均电机轴机械角速度
ElecVelocity = MechVelocity * NPP;                          //乘以极对数得到电角速度

ElecVelocityFilt = 0.99f * ElecVelocityFilt
                  + 0.01f * ElecVelocity;                  //保留原工程内部的一阶低通递推

sample->dtheta_mech = (DUAL_ENCODER_DIR / GR)
                     * MechVelocity;                       //换算为带方向的输出轴机械角速度
sample->dtheta_elec = ElecVelocity;                         //保存原工程实际提供给控制器的电角速度
sample->valid = true;                                      //本次位置和速度更新完成
}



void PositionSensor_ResetRotationState(PositionSnapshot *sample)
{
    rotations = 0;                 //清零主编码器累计跨圈数
    old_counts = sample->raw;      //沿用原工程，以当前原始计数作为跨圈判断起点
    sample->position = 0.0f;       //清零主编码器累计机械位置，随后由采样重新计算
    sample->theta_mech = 0.0f;     //清零双编码器输出轴位置，随后由采样重新计算
}





