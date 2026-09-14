#ifndef MOTOR_TYPES_H
#define MOTOR_TYPES_H


#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>



typedef struct                    //定义FOC控制命令结构体
{
    float p_des;                  //目标输出轴位置，rad
    float v_des;                  //目标输出轴速度，rad/s
    float kp;                     //位置控制增益，N·m/rad
    float kd;                     //速度控制增益，N·m·s/rad
    float t_ff;                   //前馈输出转矩，：N·m
} FocCommand;                     //完整的FOC控制命令
 


typedef struct                    //定义电机反馈结构体
{
    float p;                      //实际输出轴位置，rad
    float v;                      //实际输出轴速度，rad/s
    float t;                      //根据q轴电流估算的输出转矩，N·m
} MotorFeedback;                  //完整的电机反馈数据



typedef struct                           //定义ADC采样快照结构体
{
    uint16_t adc1_raw;                  //ADC1原始采样值
    uint16_t adc2_raw;                  //ADC2原始采样值
    uint16_t adc3_raw;                  //ADC3原始采样值
    uint32_t seq;                       //本次ADC采样所在的控制周期序号
    bool valid;                         //ADC数据是否有效，true有效，false无效
} AdcSnapshot;                          



typedef struct                           //定义位置传感器快照结构体
{
    uint16_t raw;                       //编码器1的原始角度值
    uint16_t raw2;                      //编码器2的原始角度值
    float theta_mech;                   //实际输出轴机械角度，rad
    float theta_elec;                   //电机电角度，rad
    float dtheta_mech;                  //实际输出轴机械角速度，rad/s
    float dtheta_elec;                  //电机电角速度，rad/s
    uint32_t seq;                       //本次位置采样所在的控制周期序号
    bool valid;                         //位置数据是否有效，true有效，false无效
	
	  int32_t angle;                      //编码器1经过LUT校正后的角度计数
	  float position;                     // 主编码器累计机械角度，未扣除机械零位偏置，单位rad
} PositionSnapshot;  



typedef struct
{
    AdcSnapshot adc;                    //本轮ADC输入，使用前检查valid

    float i_a;                         //A相电流，A
    float i_b;                         //B相电流，A
    float i_c;                         //C相电流，A
    float v_bus;                       //直流母线电压，V

    float i_d;                         //d轴电流，A
    float i_q;                         //q轴电流，A
    float i_q_filt;                    //滤波后的q轴电流，A
    float i_d_filt;                    //滤波后的d轴电流，A

    float v_d;                         //d轴电压，V
    float v_q;                         //q轴电压，V

    float dtc_u;                       //U相占空比
    float dtc_v;                       //V相占空比
    float dtc_w;                       //W相占空比

    float v_u;                         //U相电压，V
    float v_v;                         //V相电压，V
    float v_w;                         //W相电压，V

			float k_d;                         //d轴电流环比例系数
    float k_q;                         //q轴电流环比例系数
    float ki_d;                        //d轴电流环积分系数
    float ki_q;                        //q轴电流环积分系数
    float alpha;                       //电流参考值滤波系数

    float d_int;                       //d轴电流环积分项
    float q_int;                       //q轴电流环积分项

    int32_t adc1_offset;               //ADC1电流零偏
    int32_t adc2_offset;               //ADC2电流零偏

    float i_d_ref;                     //d轴电流参考值，A
    float i_q_ref;                     //q轴电流参考值，A
    float i_d_ref_filt;                //滤波后的d轴电流参考值，A
    float i_q_ref_filt;                //滤波后的q轴电流参考值，A

    uint32_t loop_count;               //FOC控制循环计数
    volatile uint32_t timeout;         //CAN超时计数，单位为25微秒控制周期

    float v_ref;                       //dq电压矢量幅值，V
    float fw_int;                      //弱磁控制积分项，A
} ControllerStruct;                    //FOC控制器运行状态



typedef struct
{
    double temperature;                //热模型估算温度，单位℃
    double temperature2;               //根据相电阻估算的温度，单位℃
    float resistance;                  //估算的电机相电阻，单位Ω
} ObserverStruct;                      //电机温度和电阻观测状态



typedef struct
{
    uint16_t fsr1;                      //DRV8323故障状态寄存器1原始值
    uint16_t fsr2;                      //DRV8323故障状态寄存器2原始值
} DrvFault;  



typedef struct                           //定义电机参数结构体
{
    float E_OFFSET;                      //编码器电角度偏置，rad，原工程默认0
    float M_OFFSET;                      //编码器机械零位偏置，rad，原工程默认0
    float I_BW;                          //电流环带宽，Hz，原设置范围100～2000，默认1000
    float I_MAX;                         //最大电流，A，范围0～I_MAX_MOTOR，默认I_MAX_MOTOR
    float THETA_MIN;                     //原工程保留的最小位置参数，rad，实际控制未使用
    float THETA_MAX;                     //原工程保留的最大位置参数，rad，实际控制未使用
    float I_FW_MAX;                      //最大弱磁电流，A，范围0～I_MAX_MOTOR，默认0

    int32_t PHASE_ORDER;                 //相序标志，0交换V/W相，非0使用原相序，由标定确定
    int32_t CAN_ID;                      //本机CAN节点ID，原菜单标注范围0～127，默认1
    int32_t CAN_MASTER;                  //CAN主站ID，原菜单标注范围0～127，默认0
    int32_t CAN_TIMEOUT;                 //CAN超时阈值，单位为25微秒控制周期，0表示关闭超时

    int32_t reserved_int[2];             //现有结构中的保留位置，对应旧整数数组下标4和5

    int32_t RAW_OFFSET;                  //主编码器零位的原始角度计数，14位有效值为0～16383
    int32_t RAW2_OFFSET;                 //副编码器零位的原始角度计数，14位有效值为0～16383
    int32_t ENCODER_LUT[128];            //主编码器128项有符号角度修正值，单位为编码器计数
} MotorParameters; 




typedef struct                            //定义本控制周期的FOC采样输入
{
    AdcSnapshot adc;                      //本周期三路ADC采样结果
    PositionSnapshot position;            //本周期编码器角度和速度结果
} FocInput;

typedef struct                            //定义FOC计算得到的三相占空比输出
{
    float dtc_u;                          //U相占空比，无量纲
    float dtc_v;                          //V相占空比，无量纲
    float dtc_w;                          //W相占空比，无量纲
} FocOutput;






#endif










