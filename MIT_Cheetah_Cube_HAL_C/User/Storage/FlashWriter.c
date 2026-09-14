#include "FlashWriter.h"

#include "FlashWriter.h"

HAL_StatusTypeDef FlashWriter_Open(void)
{
    FLASH_EraseInitTypeDef erase = {0};    //保存HAL扇区擦除配置
    uint32_t sector_error;                 //接收HAL返回的擦除失败扇区
    HAL_StatusTypeDef status;              //保存本次Flash操作结果

    status = HAL_FLASH_Unlock();           //FLASH_Unlock

    if (status != HAL_OK)
    {
        return status;                     //解锁失败时结束本次操作
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP
        | FLASH_FLAG_OPERR
        | FLASH_FLAG_WRPERR
        | FLASH_FLAG_PGAERR
        | FLASH_FLAG_PGPERR
        | FLASH_FLAG_PGSERR);              //擦除前清除操作完成及错误标志

    erase.TypeErase = FLASH_TYPEERASE_SECTORS; //按扇区擦除
    erase.Sector = FLASH_SECTOR_6;             //仅擦除原工程参数所在的Sector 6
    erase.NbSectors = 1U;                      //本次只擦除一个扇区
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3; //沿用原工程2.7～3.6V的擦除电压档位

    status = HAL_FLASHEx_Erase(&erase,&sector_error);                    //对应原工程的FLASH_EraseSector

    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();                  //擦除失败时重新锁定Flash
    }

    return status;                         //成功时保持解锁，供后续写入参数
}

HAL_StatusTypeDef FlashWriter_Write(uint32_t address, uint32_t data)
{
    if ((address < ADDR_FLASH_SECTOR_6) || (address > (ADDR_FLASH_SECTOR_7 - 4U)) || ((address & 3U) != 0U))
    {
        return HAL_ERROR;                  //拒绝Sector 6之外或未按4字节对齐的地址
    }

    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,data);                             //对应原工程的FLASH_ProgramWord
}

HAL_StatusTypeDef FlashWriter_Close(void)
{
    return HAL_FLASH_Lock();               //对应原工程的FLASH_Lock
}

