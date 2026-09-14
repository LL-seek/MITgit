#ifndef CAN_COM_H
#define CAN_COM_H

#include <stdint.h>                               
#include "motor_types.h"                          

void pack_reply(uint8_t data[6],uint8_t can_id,const MotorFeedback *feedback);   //将电机反馈编码为6字节MIT反馈帧

void unpack_cmd(const uint8_t data[8],FocCommand *command);                      //将8字节MIT控制帧解码为FOC控制命令

uint8_t unpack_special_cmd(const uint8_t data[8]);    //识别特殊帧，返回命令尾字节，0表示非特殊帧

#endif