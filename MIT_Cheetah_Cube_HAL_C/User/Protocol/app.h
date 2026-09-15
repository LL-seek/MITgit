#ifndef APP_H
#define APP_H

#define REST_MODE         0                  //沿用原工程的停止和菜单模式
#define CALIBRATION_MODE  1                  //沿用原工程的编码器标定模式
#define MOTOR_MODE        2                  //沿用原工程的电机运行模式
#define SETUP_MODE        4                  //沿用原工程的参数设置模式
#define ENCODER_MODE      5                  //沿用原工程的编码器显示模式

extern volatile int state;                   //当前实际运行模式

extern int can_mode_request;                 //CAN或串口的模式切换请求，-1表示无请求，其余使用上面的模式编号
extern int can_zero_request;                 //置零请求，0表示无请求，1表示请求置零
extern int can_id_request;                   //待设置的本机CAN ID，-1表示无请求，有效值为0～127
extern int can_master_request;               //待设置的主站ID，-1表示无请求，有效值为0～255

#endif
