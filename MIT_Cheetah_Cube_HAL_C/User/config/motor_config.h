#ifndef MOTOR_CONFIG_H                                      
#define MOTOR_CONFIG_H                                     


//5636电机电气参数

#define R_PHASE             2.06f                           //电机相电阻， ohm
#define L_D                 0.000245f                       //电机d轴电感， H
#define L_Q                 0.000366f                       //电机q轴电感， H

#define KT                  0.0875f                         //电机转矩常数， N*m/A
#define NPP                 14                              //电机极对数
#define GR                  36.0f                           //电机减速比，无量纲
#define KT_OUT              3.15f                           //输出端转矩常数， N*m/A
#define WB                  0.00535f                        //电机反电势常数， V*s/rad


//电机额定值和限制

#define V_NOMINAL           48.0f                           //电机额定母线电压， V
#define I_RATED             3.8f                            //电机额定电流， A
#define I_MAX_MOTOR         12.0f                           //电机允许的峰值电流， A
#define T_RATED             0.35f                           //电机额定转矩， N*m
#define T_MAX_MOTOR         1.05f                           //电机峰值转矩， N*m
#define W_RATED_RPM         3100.0f                         //电机额定转速， rpm
#define BEMF_RATED          29.8f                           //额定转速下的反电势， V RMS


//双编码器齿轮参数

#define ENCODER_MAIN_TEETH  35.0f                           //主编码器齿轮齿数
#define ENCODER_SUB_TEETH   36.0f                           //副编码器齿轮齿数
#define DUAL_ENCODER_DIR    1.0f                            //双编码器方向，正向为1.0，反向为-1.0


//电机热模型参数

#define R_TH                1.25f                           //电机热阻， K/W
#define INV_M_TH            0.03125f                        //电机逆热容量， K/J


#endif                                                       