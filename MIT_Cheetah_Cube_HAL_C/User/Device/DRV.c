#include "DRV.h"                                                                                              
#include "spi.h"                                                                                              
#include "main.h"                                                                                             

#define DRV_SPI_TIMEOUT_MS  2U                                                                                //设置SPI收发的超时参数为2ms

HAL_StatusTypeDef DRV_Transfer16(uint16_t tx_data, uint16_t *rx_data)                                            //发送并接收一个16位数据，返回HAL状态
{
    HAL_StatusTypeDef status;                                                                                    //保存本次SPI收发的返回状态

    HAL_GPIO_WritePin(DRV_CS_N_GPIO_Port, DRV_CS_N_Pin, GPIO_PIN_RESET);                                         //拉低片选CS，开始本次SPI事务

    status = HAL_SPI_TransmitReceive(&hspi1, (const uint8_t *)&tx_data, (uint8_t *)rx_data, 1U, DRV_SPI_TIMEOUT_MS); //SPI1收发1个16位数据单元，超时参数为2ms

    HAL_GPIO_WritePin(DRV_CS_N_GPIO_Port, DRV_CS_N_Pin, GPIO_PIN_SET);                                           //HAL返回后，无论成功失败都拉高CS，结束事务

    return status;                                                                                               //返回收发状态，只有HAL_OK时接收数据才有效
}



HAL_StatusTypeDef read_register(uint8_t reg, uint16_t *val)          //读取reg指定的寄存器，结果写入*val，返回HAL状态
{
    uint16_t tx_data;                                                //保存拼接后的16位SPI读命令
    HAL_StatusTypeDef status;                                        //保存本次SPI通信的返回状态

    tx_data = (uint16_t)(0x8000U | ((reg & 0x0FU) << 11U));          //bit15置1表示读，取reg低4位并左移11位作为寄存器地址

    status = DRV_Transfer16(tx_data, val);                           //发送读命令，同时将接收到的数据写入val指向的变量

    if (status == HAL_OK)                                            //通信成功时，提取有效的寄存器数据
    {
        *val = (uint16_t)(*val & 0x07FFU);                           //保留低11位有效数据，清除高5位
    }

    return status;                                                   //返回本次SPI通信的状态
}



HAL_StatusTypeDef write_register(uint8_t reg, uint16_t val)          //向reg指定的寄存器写入val，返回HAL状态
{
    uint16_t tx_data;                                                //保存拼接后的16位SPI写命令
    uint16_t rx_data;                                                //保存本次SPI通信中芯片返回的数据

    tx_data = (uint16_t)(((reg & 0x0FU) << 11U) |                    //取reg低4位并左移11位，放入bit14～11
                         (val & 0x07FFU));                           //取val低11位作为数据，bit15保持0表示写操作

    return DRV_Transfer16(tx_data, &rx_data);                        //发送写命令，将返回数据存入rx_data，并返回HAL状态
}



HAL_StatusTypeDef write_DCR(uint8_t DIS_CPUV, uint8_t DIS_GDF,          //配置DCR寄存器，将各配置字段组合后写入，返回HAL状态
                           uint8_t OTW_REP, uint8_t PWM_MODE,
                           uint8_t PWM_COM, uint8_t PWM_DIR,
                           uint8_t COAST, uint8_t BRAKE,
                           uint8_t CLR_FLT)
{
    uint16_t val;                                                       //保存拼接后的DCR寄存器配置数据

    val = (uint16_t)((DIS_CPUV << 9U) |                                 //将各配置字段左移到对应位，再按位或组合成11位寄存器数据
                     (DIS_GDF  << 8U) |
                     (OTW_REP  << 7U) |
                     (PWM_MODE << 5U) |
                     (PWM_COM  << 4U) |
                     (PWM_DIR  << 3U) |
                     (COAST    << 2U) |
                     (BRAKE    << 1U) |
                     CLR_FLT);

    return write_register(DCR, val);                                    //将val写入DCR寄存器，返回本次SPI通信的状态
}     



HAL_StatusTypeDef write_HSR(uint8_t LOCK, uint8_t IDRIVEP_HS,          //配置高侧栅极驱动寄存器HSR，返回HAL状态
                           uint8_t IDRIVEN_HS)
{
    uint16_t val;                                                     //保存拼接后的HSR寄存器配置数据

    val = (uint16_t)((LOCK       << 8U) |                             //LOCK左移8位，IDRIVEP_HS左移4位，再与IDRIVEN_HS按位或组合
                     (IDRIVEP_HS << 4U) |
                     IDRIVEN_HS);

    return write_register(HSR, val);                                  //将val写入HSR寄存器，返回本次SPI通信的状态
}

HAL_StatusTypeDef write_LSR(uint8_t CBC, uint8_t TDRIVE,               //配置低侧栅极驱动寄存器LSR，返回HAL状态
                           uint8_t IDRIVEP_LS, uint8_t IDRIVEN_LS)
{
    uint16_t val;                                                   //保存拼接后的LSR寄存器配置数据

    val = (uint16_t)((CBC        << 10U) |                           //将CBC、TDRIVE和IDRIVEP_LS分别左移10、8、4位，再与IDRIVEN_LS按位或组合
                     (TDRIVE     << 8U)  |
                     (IDRIVEP_LS << 4U)  |
                     IDRIVEN_LS);

    return write_register(LSR, val);                                //将val写入LSR寄存器，返回本次SPI通信的状态
}



HAL_StatusTypeDef write_OCPCR(uint8_t TRETRY, uint8_t DEAD_TIME,       //配置过流保护控制寄存器OCPCR，返回HAL状态
                             uint8_t OCP_MODE, uint8_t OCP_DEG,
                             uint8_t VDS_LVL)
{
    uint16_t val;                                                   //保存拼接后的OCPCR寄存器配置数据

    val = (uint16_t)((TRETRY    << 10U) |                            //将TRETRY、DEAD_TIME、OCP_MODE和OCP_DEG分别左移10、8、6、4位，再与VDS_LVL按位或组合
                     (DEAD_TIME << 8U)  |
                     (OCP_MODE  << 6U)  |
                     (OCP_DEG   << 4U)  |
                     VDS_LVL);

    return write_register(OCPCR, val);                              //将val写入OCPCR寄存器，返回本次SPI通信的状态
}



HAL_StatusTypeDef write_CSACR(uint8_t CSA_FET, uint8_t VREF_DIV,
                             uint8_t LS_REF, uint8_t CSA_GAIN,
                             uint8_t DIS_SEN, uint8_t CSA_CAL_A,
                             uint8_t CSA_CAL_B, uint8_t CSA_CAL_C,
                             uint8_t SEN_LVL)
{
    uint16_t val;

    val = (uint16_t)((CSA_FET   << 10U) |  // bit10：CSA 正输入选择
                     (VREF_DIV  << 9U)  |  // bit9：参考电压选择
                     (LS_REF    << 8U)  |  // bit8：低侧 VDS 检测参考选择
                     (CSA_GAIN  << 6U)  |  // bit7:6：CSA 增益
                     (DIS_SEN   << 5U)  |  // bit5：采样过流保护禁用位
                     (CSA_CAL_A << 4U)  |  // bit4：A 相 CSA 校准控制
                     (CSA_CAL_B << 3U)  |  // bit3：B 相 CSA 校准控制
                     (CSA_CAL_C << 2U)  |  // bit2：C 相 CSA 校准控制
                     SEN_LVL);             // bit1:0：采样过流检测阈值

    return write_register(CSACR, val);
}



HAL_StatusTypeDef DRV_Init(void)                 //唤醒DRV8323，设置COAST并清故障，返回HAL状态
{
    HAL_StatusTypeDef status;                   //保存本次DCR写入的HAL返回状态

    HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port,     //DRV使能信号所在的GPIO端口
                      DRV_ENABLE_Pin,           //DRV使能引脚
                      GPIO_PIN_SET);            //拉高ENABLE，唤醒DRV8323

    HAL_Delay(1U);                              //等待至少1ms，再开始首次SPI通信

    status = write_DCR(DIS_CPUV_EN,             //启用电荷泵欠压故障保护
                       DIS_GDF_EN,              //启用栅极驱动故障保护
                       OTW_REP_DIS,             //不通过nFAULT引脚和FAULT位上报过温预警
                       PWM_MODE_3X,             //选择3路PWM输入模式
                       PWM_1X_COM_SYNC,         //1路PWM模式下选择同步整流
                       PWM_1X_DIR_0,            //1路PWM模式的方向控制位设为0
                       1U,                      //COAST：关断全部MOSFET，功率输出进入高阻态
                       0U,                      //BRAKE：不强制制动
                       1U);                     //CLR_FLT：请求清除锁存故障

if (status == HAL_OK)                        //DCR写入的HAL返回成功后，配置电流采样放大器
{
    status = write_CSACR(CSA_FET_SP,         //选择SPx作为电流采样放大器的正输入
                         VREF_DIV_2,         //参考电压选择VREF/2
                         0U,                 //LS_REF：低侧VDS检测参考选择位设为0
                         CSA_GAIN_40,        //电流采样放大器增益设为40倍
                         DIS_SEN_EN,         //启用电流采样过流检测
                         0U,                 //CSA_CAL_A：不启用A相CSA校准
                         0U,                 //CSA_CAL_B：不启用B相CSA校准
                         0U,                 //CSA_CAL_C：不启用C相CSA校准
                         SEN_LVL_1_0);       //电流采样过流检测阈值设为1.0V
}

if (status == HAL_OK)                        //CSA配置写入的HAL返回成功后，配置过流保护
{
    status = write_OCPCR(TRETRY_4MS,         //过流故障自动重试时间设为4ms
                         DEADTIME_200NS,     //死区时间设为200ns
                         OCP_RETRY,          //过流保护选择自动重试模式
                         OCP_DEG_8US,        //过流检测去毛刺时间设为8us
                         VDS_LVL_1_88);      //MOSFET漏源电压VDS的过流检测阈值设为1.88V
}

	
if (status != HAL_OK)                       //如果本次SPI传输未成功
{
    HAL_GPIO_WritePin(DRV_ENABLE_GPIO_Port, //DRV使能信号所在的GPIO端口
                          DRV_ENABLE_Pin,       //DRV使能引脚
                          GPIO_PIN_RESET);      //拉低ENABLE，关闭驱动芯片
}

return status;                              //返回本次DCR写入的HAL状态

}




