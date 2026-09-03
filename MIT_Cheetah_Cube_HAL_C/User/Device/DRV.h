#ifndef DRV_H                                                              
#define DRV_H                                                               

#include "stm32f4xx_hal.h"                                                   
#include <stdint.h>      


#define FSR1    0x00U                                           //故障状态寄存器1地址
#define FSR2    0x01U                                           //故障状态寄存器2地址
#define DCR     0x02U                                           //驱动控制寄存器地址
#define HSR     0x03U                                           //高侧栅极驱动寄存器地址
#define LSR     0x04U                                           //低侧栅极驱动寄存器地址
#define OCPCR   0x05U                                           //过流保护控制寄存器地址
#define CSACR   0x06U                                           //电流检测放大器控制寄存器地址

#define DIS_CPUV_EN         0x0U        //启用电荷泵欠压故障保护
#define DIS_GDF_EN          0x0U        //启用栅极驱动故障保护
#define OTW_REP_DIS         0x0U        //不通过nFAULT引脚和FAULT位上报过温预警
#define PWM_MODE_3X         0x1U        //选择3路PWM输入模式
#define PWM_1X_COM_SYNC     0x0U        //1路PWM模式采用同步整流
#define PWM_1X_DIR_0        0x0U        //1路PWM模式的方向控制位设为0

#define CSA_FET_SP          0x0U        //电流采样放大器的正输入选择SPx引脚
#define VREF_DIV_2          0x1U        //电流采样放大器的参考电压选择VREF/2
#define CSA_GAIN_40         0x3U        //电流采样放大器的增益选择40倍
#define DIS_SEN_EN          0x0U        //启用采样过流故障检测
#define SEN_LVL_1_0         0x3U        //采样过流检测电压阈值选择1.0V

#define TRETRY_4MS          0x0U        //过流故障自动重试时间选择4ms
#define DEADTIME_200NS      0x2U        //死区时间选择200ns
#define OCP_RETRY           0x1U        //过流保护采用自动重试模式
#define OCP_DEG_8US         0x3U        //过流检测去毛刺时间选择8us
#define VDS_LVL_1_88        0xFU        //MOSFET漏源电压VDS的过流检测阈值选择1.88V


HAL_StatusTypeDef read_register(uint8_t reg, uint16_t *val);      //读取寄存器reg，结果写入*val，返回HAL状态
HAL_StatusTypeDef write_register(uint8_t reg, uint16_t val);      //将val写入寄存器reg，返回HAL状态

HAL_StatusTypeDef DRV_Transfer16(uint16_t tx_data, uint16_t *rx_data);          //发送tx_data并将接收值写入rx_data，返回HAL_OK时接收数据有效

HAL_StatusTypeDef write_DCR(                // 配置驱动控制寄存器DCR，返回本次写入的HAL状态
    uint8_t DIS_CPUV,                       //电荷泵欠压故障保护：0启用，1禁用
    uint8_t DIS_GDF,                        //栅极驱动故障保护：0启用，1禁用
    uint8_t OTW_REP,                        //过温预警上报：0关闭，1开启
    uint8_t PWM_MODE,                       //PWM输入模式：0为6路，1为3路，2为1路，3为独立模式
    uint8_t PWM_COM,                        //1路PWM模式的整流方式：0同步整流，1异步整流
    uint8_t PWM_DIR,                        //1路PWM模式的方向控制位
    uint8_t COAST,                          //滑行控制：1使全部MOSFET关断，功率输出进入高阻态
    uint8_t BRAKE,                          //制动控制：0不强制制动，1请求制动
    uint8_t CLR_FLT);                       //清故障控制：写1请求清除锁存故障


HAL_StatusTypeDef write_HSR(uint8_t LOCK, uint8_t IDRIVEP_HS, uint8_t IDRIVEN_HS);         //配置HSR寄存器，设置锁定状态和高侧栅极开通、关断驱动电流，返回HAL状态
                           
HAL_StatusTypeDef write_LSR(uint8_t CBC, uint8_t TDRIVE, uint8_t IDRIVEP_LS, uint8_t IDRIVEN_LS);              //配置LSR寄存器，设置逐周期故障恢复、峰值驱动时间和低侧栅极驱动电流，返回HAL状态
                           
HAL_StatusTypeDef write_OCPCR(uint8_t TRETRY, uint8_t DEAD_TIME,  uint8_t OCP_MODE, uint8_t OCP_DEG, uint8_t VDS_LVL);     //配置过流保护控制寄存器OCPCR，设置保护参数及死区时间，返回HAL状态
                            
HAL_StatusTypeDef write_CSACR(uint8_t CSA_FET, uint8_t VREF_DIV, uint8_t LS_REF, uint8_t CSA_GAIN,uint8_t DIS_SEN, uint8_t CSA_CAL_A,uint8_t CSA_CAL_B, uint8_t CSA_CAL_C, uint8_t SEN_LVL);      //配置电流采样放大器控制寄存器CSACR，设置采样、校准及过流检测参数，返回HAL状态
                             
HAL_StatusTypeDef DRV_Init(void);    //启动DRV8323：拉高ENABLE、等待就绪、设置COAST并清故障，返回HAL状态

HAL_StatusTypeDef calibrate(void);    //执行DRV8323电流采样放大器CSA校准，返回HAL状态
#endif     






