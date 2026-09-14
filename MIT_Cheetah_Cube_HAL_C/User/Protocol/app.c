#include "app.h"
volatile int state = REST_MODE;             //沿用原工程，上电默认处于停止模式
int can_mode_request = -1;                    //启动时没有模式切换请求
int can_zero_request = 0;                     //启动时没有置零请求
int can_id_request = -1;                      //启动时没有本机CAN ID修改请求
int can_master_request = -1;                  //启动时没有主站ID修改请求