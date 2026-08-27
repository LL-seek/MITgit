#include "CAN_com.h"

 #define P_MIN -12.5f
 #define P_MAX 12.5f
 #define V_MIN -10.0f
 #define V_MAX 10.0f
 #define KP_MIN 0.0f
 #define KP_MAX 500.0f
 #define KD_MIN 0.0f
 #define KD_MAX 5.0f
 #define T_MIN -36.0f
 #define T_MAX 36.0f
 
void pack_reply(CANMessage *msg, float p, float v, float t){
	  //2026.6.5添加
	  msg->id = CAN_MASTER;
	  //
    // 回包前先限幅，防止超过协议范围后位打包溢出。
    p = fmaxf(fminf(p, P_MAX), P_MIN);
    v = fmaxf(fminf(v, V_MAX), V_MIN);
    t = fmaxf(fminf(t, T_MAX), T_MIN);
    int p_int = float_to_uint(p, P_MIN, P_MAX, 16);
    int v_int = float_to_uint(v, V_MIN, V_MAX, 12);
    int t_int = float_to_uint(t, T_MIN, T_MAX, 12);
    msg->data[0] = CAN_ID;
    msg->data[1] = p_int>>8;
    msg->data[2] = p_int&0xFF;
    msg->data[3] = v_int>>4;
    msg->data[4] = ((v_int&0xF)<<4) + (t_int>>8);
    msg->data[5] = t_int&0xFF;
    }
    
void unpack_cmd(CANMessage msg, ControllerStruct * controller){        //上位机协议处理之后，得到8bite，64bit，然后再在这一步进行，这里的字节和上位机处理的是一样的
        int p_int = (msg.data[0]<<8)|msg.data[1];                      //data[0]变成高八位 data[1]变成低8位，组成16位p_int
        int v_int = (msg.data[2]<<4)|(msg.data[3]>>4);                 //data[2]是8位bit，左移4位变成12位 这里需要的是data[3]的高八位，所以右移4位，然后按位与组成12位置
        int kp_int = ((msg.data[3]&0xF)<<8)|msg.data[4];               //data[3]是8位bit，先清楚高4位，保留低4位，然后左移变成12位，data[4]是8位，扩展成12位就是高4位变成-，低8位是原来的，在按位与
        int kd_int = (msg.data[5]<<4)|(msg.data[6]>>4);                //同上
        int t_int = ((msg.data[6]&0xF)<<8)|msg.data[7];                //同上
        
        controller->p_des = uint_to_float(p_int, P_MIN, P_MAX, 16);    //这个是映射，由编码转换成float的过程中，需要映射，比如位置，为什么位置需要2的16次方呢，因为这样精度更高，P_MIN代表0，P_MAX代表2的16次方-1，则p_des从这区间映射
        controller->v_des = uint_to_float(v_int, V_MIN, V_MAX, 12);
        controller->kp = uint_to_float(kp_int, KP_MIN, KP_MAX, 12);
        controller->kd = uint_to_float(kd_int, KD_MIN, KD_MAX, 12);
        controller->t_ff = uint_to_float(t_int, T_MIN, T_MAX, 12);
	
    }

