#include "app.h"

volatile int state = REST_MODE;              //上电初始处于停止模式，沿用原工程
volatile int state_change = 0;               //初始无入口动作请求，启动结束后再置1

int can_mode_request = -1;                   //启动时没有模式切换请求
int can_zero_request = 0;                    //启动时没有置零请求
int can_id_request = -1;                     //启动时没有本机CAN ID修改请求
int can_master_request = -1;                 //启动时没有主站ID修改请求







