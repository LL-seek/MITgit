#ifndef __FLASHWRITER_H
#define __FLASHWRITER_H

#include "stm32f4xx_flash.h"

#define ADDR_FLASH_SECTOR_0 ((uint32_t)0x08000000)  // Sector 0起始地址，大小16KB（中间的部分是sector）
#define ADDR_FLASH_SECTOR_1 ((uint32_t)0x08004000)  // Sector 1起始地址，大小16KB
#define ADDR_FLASH_SECTOR_2 ((uint32_t)0x08008000)  // Sector 2起始地址，大小16KB
#define ADDR_FLASH_SECTOR_3 ((uint32_t)0x0800C000)  // Sector 3起始地址，大小16KB
#define ADDR_FLASH_SECTOR_4 ((uint32_t)0x08010000)  // Sector 4起始地址，大小64KB
#define ADDR_FLASH_SECTOR_5 ((uint32_t)0x08020000)  // Sector 5起始地址，大小128KB
#define ADDR_FLASH_SECTOR_6 ((uint32_t)0x08040000)  // Sector 6起始地址，大小128KB
#define ADDR_FLASH_SECTOR_7 ((uint32_t)0x08060000)  // Sector 7起始地址，大小128KB

static uint32_t __SECTOR_ADDRS[] = {ADDR_FLASH_SECTOR_0, ADDR_FLASH_SECTOR_1, ADDR_FLASH_SECTOR_2, ADDR_FLASH_SECTOR_3,
                             ADDR_FLASH_SECTOR_4, ADDR_FLASH_SECTOR_5, ADDR_FLASH_SECTOR_6, ADDR_FLASH_SECTOR_7};
static uint32_t __SECTORS[] = {FLASH_Sector_0, FLASH_Sector_1, FLASH_Sector_2, FLASH_Sector_3,
                             FLASH_Sector_4, FLASH_Sector_6, FLASH_Sector_6, FLASH_Sector_7};
class FlashWriter {
public:
    FlashWriter(int sector);
    void open();
    bool ready();
    void write(uint32_t index, int x);
    void write(uint32_t index, unsigned int x);
    void write(uint32_t index, float x);
    void close();
private:
    uint32_t __base;       //是目标扇区的起始地址
    uint32_t __sector;
    bool __ready;
};

int flashReadInt(uint32_t sector, uint32_t index);
uint32_t flashReadUint(uint32_t sector, uint32_t index);
float flashReadFloat(uint32_t sector, uint32_t index);
    
#endif

