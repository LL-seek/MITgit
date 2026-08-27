#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H

// 5636 电机参数
#define R_PHASE 2.06f           // 相电阻，欧姆
#define L_D 0.000245f           // d轴电感，亨利
#define L_Q 0.000366f           // q轴电感，亨利

#define KT 0.0875f              // 转矩常数，N-m/A，由峰值扭矩1.05Nm和峰值电流12A估算
#define NPP 14                  // 极对数
#define GR 36.0f                // 减速比，由36齿和35齿双编码器齿差得到
#define KT_OUT 3.15f            // 输出端转矩常数，KT*GR
#define WB 0.00535f             // 磁链，按29.8V线电压RMS、3100rpm换算

#define V_NOMINAL 48.0f         // 额定母线电压，伏
#define I_RATED 3.8f            // 额定电流，安
#define I_MAX_MOTOR 12.0f       // 峰值电流，安
#define T_RATED 0.35f           // 额定扭矩，N-m
#define T_MAX_MOTOR 1.05f       // 峰值扭矩，N-m
#define W_RATED_RPM 3100.0f     // 额定转速，rpm
#define BEMF_RATED 29.8f        // 额定转速对应反电势，伏

// 双编码器齿轮参数
#define ENCODER_MAIN_TEETH 35.0f    // 主编码器齿轮齿数
#define ENCODER_SUB_TEETH 36.0f     // 副编码器齿轮齿数
#define DUAL_ENCODER_DIR 1.0f       // 双编码器输出方向，方向反时改为-1.0f

#define R_TH 1.25f              // 热阻，K/W
#define INV_M_TH 0.03125f       // 热容倒数，K/J

#endif