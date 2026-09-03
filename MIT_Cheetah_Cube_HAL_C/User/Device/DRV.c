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




