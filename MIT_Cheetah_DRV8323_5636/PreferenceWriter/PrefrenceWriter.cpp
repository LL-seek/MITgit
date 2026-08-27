#include "PreferenceWriter.h"
#include "FlashWriter.h"
#include "user_config.h"
#include "mbed.h"

PreferenceWriter::PreferenceWriter(uint32_t sector) {
    writer = new FlashWriter(sector);
    __sector = sector;
    __ready = false;
}

void PreferenceWriter::open() {
    writer->open();
    __ready = true;
}

bool  PreferenceWriter::ready() {
    return __ready;
}

void PreferenceWriter::write(int x, int index) {
    __int_reg[index] = x;                                 //将整数x暂存到RAM中第index个整数参数位置，尚未写入Flash
}

void PreferenceWriter::write(float x, int index) {
    __float_reg[index] = x;                               //将浮点数x暂存到RAM中第index个整数参数位置，尚未写入Flash
}



void PreferenceWriter::flush() {                          //将RAM中的全部参数写入Flash
    int offs;                                             //Flash参数槽位编号

    for(offs = 0; offs < 256; offs++) {                   //遍历整数参数槽位 0~255
        writer->write(offs, __int_reg[offs]);             //将第offs个整数参数写入第offs个Flash 槽位
    }

    for(; offs < 320; offs++) {                           //此时offs已是256，继续遍历槽位256~319
        writer->write(offs, __float_reg[offs - 256]);     //将第0~63个浮点参数写入第256~319个Flash槽位
    }

    __ready = false;                                      //标记本次保存流程结束，不会在这里锁定Flash
}

void PreferenceWriter::load() {                           //从flash加载全部参数到RAM数组
    int offs;                                             //Flash中的32位参数槽位编号
    for (offs = 0; offs < 256; offs++) {                  //读取槽位0～255的256个整数参数
        __int_reg[offs] = flashReadInt(__sector, offs);   //从指定扇区读取整数并存入整数参数数组
    }
    for(; offs < 320; offs++) {
        __float_reg[offs - 256] = flashReadFloat(__sector, offs);      //同上，不过这个是浮点数，读取的槽位是256-319
    }
}

void PreferenceWriter::close() {
    __ready = false;
    writer->close();
}
