#ifndef APP_H
#define APP_H

#define REST_MODE  0                          //沿用原工程的停止模式编号
#define MOTOR_MODE 2                          //沿用原工程的电机模式编号
extern volatile int state;                  //当前实际运行模式，沿用原工程状态变量
extern int can_mode_request;                  //待切换模式，-1表示无请求，0停止，2进入电机模式
extern int can_zero_request;                  //沿用原工程置零请求，0无请求，1请求置零
extern int can_id_request;                    //待设置的本机CAN ID，-1表示无请求，有效值0～127
extern int can_master_request;                //待设置的主站ID，-1表示无请求，有效值0～255

#endif


