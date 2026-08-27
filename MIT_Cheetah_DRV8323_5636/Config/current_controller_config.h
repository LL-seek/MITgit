#ifndef CURRENT_CONTROLLER_CONFIG_H
#define CURRENT_CONTROLLER_CONFIG_H

// 电流环参数
#define K_SCALE 0.0001f             // 电流环带宽到环路增益的比例系数

#define V_BUS 48.0f                 // 母线电压，伏
#define OVERMODULATION 1.15f        // 过调制系数，1.0表示不过调制

#define D_INT_LIM V_BUS/(K_D*KI_D)  // d轴积分限幅
#define Q_INT_LIM V_BUS/(K_Q*KI_Q)  // q轴积分限幅

// 观测器参数
#define DT 0.000025f                // 控制周期，秒
#define K_O 0.02f                   // 观测器增益

// 5636 电机电流环初始参数
#define K_D 0.77f                   // d轴电流环比例增益，V/A
#define K_Q 1.15f                   // q轴电流环比例增益，V/A

#define KI_D 0.21f                  // d轴电流环积分系数
#define KI_Q 0.14f                  // q轴电流环积分系数

#endif