#include "stm32f4xx_flash.h"
#include "FlashWriter.h"

FlashWriter::FlashWriter(int sector) {
    if (sector > 7) sector = 7;
    __sector = sector;
    __base = __SECTOR_ADDRS[sector];
    __ready = false;
}

bool FlashWriter::ready()            //返回Flash写入器是否已打开并处于可写状态
{
    return __ready;                  //true表示已准备好，false表示未打开或已关闭
}

void FlashWriter::open() {           //解除并擦除flash扇区
    FLASH_Unlock();                  //解锁falsh
	  //清除flash      清除操作完成标志   清除写错误标志      清除地址对齐错误标志  清除并行写入标志    清除顺序写入标志
    FLASH_ClearFlag( FLASH_FLAG_EOP |  FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    FLASH_EraseSector(__SECTORS[__sector], VoltageRange_3);           //擦除指定flash扇区
    __ready = true;                                                   //标志flash已经擦除，可以写入
}

void FlashWriter::write(uint32_t index, int x)                        //index第几个参数槽位；x要保存的整数参数
{
    union
    {
        int a;        // 用 int 类型接收待保存的整数 x
        uint32_t b;   // 与 a 共用同一块 4 字节内存，供 Flash 写入函数使用
    };

    a = x;                                       //将整数x放入这 4 个字节中

    FLASH_ProgramWord(__base + 4 * index, b);
    // __base           ：Flash 参数区的起始地址，例如 0x08040000
    // 4 * index        ：每个参数占 4 字节，计算第 index 个参数的位置
    // b                ：取出 x 在内存中的原始 32 位数据
    // FLASH_ProgramWord：将这 32 位数据真正写入 Flash
}

void FlashWriter::write(uint32_t index, unsigned int x) {             //向flash写入一个无符号的32位数，例如（2，100）就是把100写到第二个位置上
    FLASH_ProgramWord(__base + 4 * index, x);                         //（目标地址，写入的数据）
}

void FlashWriter::write(uint32_t index, float x)                      //index：Flash 中第几个 4 字节槽位；x：要保存的浮点参数
{
    union
    {
        float a;      // 用 float 形式存放参数值，例如 1.5f
        uint32_t b;   // 与 a 共用同一组 4 字节，用于取得这 4 字节的原始二进制数据
    };

    a = x;
    // 将浮点数 x 写入这 4 个字节。
    // 这里不是把 1.5f 转成整数 1，而是保留 1.5f 的 IEEE-754 32 位二进制表示。

    FLASH_ProgramWord(__base + 4 * index, b);
    // __base：Flash 参数区起始地址，例如 0x08040000。
    // 4 * index：每个参数槽位占 4 字节，计算第 index 个槽位的偏移量。
    // b：以 uint32_t 形式传入 float 的原始 32 位数据。
    // FLASH_ProgramWord：把这 32 位数据真正写入 Flash。
}

void FlashWriter::close() {
    FLASH_Lock();
    __ready = false;
}

int flashReadInt(uint32_t sector, uint32_t index) {                  //从指定扇区读取编号为index的整数
    return *(int*) (__SECTOR_ADDRS[sector] + 4 * index);             //取得目标扇区的起始地址，以及每个整数占4字节，计算参数地址并读取
}   

uint32_t flashReadUint(uint32_t sector, uint32_t index) {
    return *(uint32_t*) (__SECTOR_ADDRS[sector] + 4 * index);
}   

float flashReadFloat(uint32_t sector, uint32_t index) {
    return *(float*) (__SECTOR_ADDRS[sector] + 4 * index);
}   
