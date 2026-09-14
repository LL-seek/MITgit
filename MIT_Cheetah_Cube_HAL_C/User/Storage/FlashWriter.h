#ifndef FLASH_WRITER_H
#define FLASH_WRITER_H

#include "stm32f4xx_hal.h"

#define ADDR_FLASH_SECTOR_6  0x08040000U       //6扇区的起始地址
#define ADDR_FLASH_SECTOR_7  0x08060000U       //下一扇区起始地址，作为Sector 6的结束边界

HAL_StatusTypeDef FlashWriter_Open(void);      //解锁并擦除原工程使用的Sector 6

HAL_StatusTypeDef FlashWriter_Write(
    uint32_t address,                         //待写入位置的绝对地址，必须按4字节对齐
    uint32_t data);                           //待写入的32位原始数据

HAL_StatusTypeDef FlashWriter_Close(void);    //锁定Flash

#endif