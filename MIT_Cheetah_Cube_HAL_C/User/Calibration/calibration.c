#include "calibration.h"
#include "PositionSensor.h"
#include "foc.h"
#include "motor_config.h"
#include "hw_config.h"
#include "math_ops.h"
#include "pwm.h"
#include "bsp_time.h"
#include "bsp_debug_uart.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void wait_us(uint32_t us)                         //用现有微秒时基替换mbed延时，期间保持中断运行
{
    uint32_t start_us = BSP_Time_NowUs32();

    while (BSP_Time_ElapsedUs32(start_us) < us)
    {
    }
}

void order_phases(const volatile PositionSnapshot *ps,
                  const volatile ControllerStruct *controller,
                  MotorParameters *parameters)
{
    float theta_ref = 0.0f;                              //旋转磁场的电角度指令，rad
    float theta_actual;
    float theta_start = 0.0f;
    float theta_end;
    float v_d = V_CAL;
    float v_q = 0.0f;
    float v_u, v_v, v_w;
    float dtc_u, dtc_v, dtc_w;
    float i_a, i_b, i_c, i_d, i_q;
    float theta_elec;
    float current;
    int sample_counter = 0;
    int direction;
    uint32_t primask;
    char text[128];

    BSP_DebugUart_TryWrite("\r\nChecking phase ordering\r\n");
    abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);
    svm(1.0f, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);

    PWM_SetCompare(0.5f * (1.0f + dtc_u),                //换算为现有PWM接口的占空比，保留原式ARR/2的标定幅值
                   0.5f * (1.0f + dtc_v),
                   0.5f * (1.0f + dtc_w));               //相序判断时固定使用原来的U、V、W通道顺序
    HAL_Delay(2000U);                                    //沿用原工程约2秒的固定磁场对齐
    wait_us(1000U);

    primask = __get_PRIMASK();
    __disable_irq();                                    //一次读取同一组ADC结果和编码器电角度
    i_b = I_SCALE * (float)((int32_t)controller->adc.adc2_raw - controller->adc2_offset);
    i_c = I_SCALE * (float)((int32_t)controller->adc.adc1_raw - controller->adc1_offset);
    theta_elec = ps->theta_elec;
    __set_PRIMASK(primask);

    i_a = -i_b - i_c;
    dq0(theta_elec, i_a, i_b, i_c, &i_d, &i_q);
    current = sqrtf(i_d * i_d + i_q * i_q);
    snprintf(text, sizeof(text), "\r\nCurrent\r\n%f\t%f\t%f\r\n",
             (double)i_d, (double)i_q, (double)current);
    BSP_DebugUart_TryWrite(text);                        //保留原工程的标定电流显示

    while (theta_ref < 4.0f * PI)                        //沿用原工程的两圈电角度扫描
    {
        abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);
        svm(1.0f, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);
        PWM_SetCompare(0.5f * (1.0f + dtc_u),
                       0.5f * (1.0f + dtc_v),
                       0.5f * (1.0f + dtc_w));
        wait_us(100U);                                  //等待期间由现有25微秒中断更新编码器采样

        theta_actual = ps->position;                    //对应原工程GetMechPositionFixed，不使用减速器输出角度
        if (theta_ref == 0.0f)
        {
            theta_start = theta_actual;
        }

        if (sample_counter > 200)
        {
            sample_counter = 0;
            snprintf(text, sizeof(text), "%.4f\t%.4f\r\n",
                     (double)(theta_ref / NPP), (double)theta_actual);
            BSP_DebugUart_TryWrite(text);
        }

        sample_counter++;
        theta_ref += 0.001f;                             //沿用原工程每步0.001rad的电角度增量
    }

    theta_end = ps->position;
    direction = (theta_end - theta_start) > 0.0f;
    parameters->PHASE_ORDER = direction;                 //正向为1，反向为0，后续扫描按此交换V/W
    snprintf(text, sizeof(text),
             "Theta Start: %f  Theta End: %f\r\nDirection: %d\r\n%s\r\n",
             (double)theta_start, (double)theta_end, direction,
             direction ? "Phasing correct" : "Phasing incorrect. Swapping V and W");
    BSP_DebugUart_TryWrite(text);
}

void calibrate_encoder(const volatile PositionSnapshot *ps, MotorParameters *parameters)
{
    static float error_f[128 * NPP];                     //正向误差，静态工作区替换原工程动态数组
    static float error_b[128 * NPP];                     //反向误差
    static float error[128 * NPP];                       //正反向平均误差
    static float error_filt[128 * NPP];                  //128点环形滑动平均后的误差
    static int32_t raw_f[128 * NPP];                     //正向原始编码器计数
    static int32_t raw_b[128 * NPP];                     //反向原始编码器计数
    static int32_t lut[128];                             //128项编码器补偿表，单位为编码器计数
    const int n = 128 * NPP;
    const int n2 = 40;
    const int n_lut = 128;
    const int window = 128;
    float delta = 2.0f * PI * NPP / (n * n2);            //沿用原工程每两个标定点之间细分40步
    float theta_ref = 0.0f;
    float theta_actual;
    float v_d = V_CAL;
    float v_q = 0.0f;
    float v_u, v_v, v_w;
    float dtc_u, dtc_v, dtc_w;
    float offset = 0.0f;
    float mean = 0.0f;
    int raw_offset;
    uint32_t primask;
    char text[128];

    BSP_DebugUart_TryWrite("Starting calibration procedure\r\n");
    memset(lut, 0, sizeof(lut));                          //每次标定重新清零，保持原工程new数组的初值
    primask = __get_PRIMASK();
    __disable_irq();
    PositionSensor_WriteLUT(lut);                        //短临界区内清除旧补偿，避免采样读到半张LUT
    __set_PRIMASK(primask);

    abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);
    svm(1.0f, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);
    if (parameters->PHASE_ORDER)
    {
        PWM_SetCompare(0.5f * (1.0f + dtc_u),
                       0.5f * (1.0f + dtc_v),
                       0.5f * (1.0f + dtc_w));
    }
    else
    {
        PWM_SetCompare(0.5f * (1.0f + dtc_u),
                       0.5f * (1.0f + dtc_w),
                       0.5f * (1.0f + dtc_v));           //相序反向时交换V/W，与原工程一致
    }
    HAL_Delay(4000U);                                    //沿用原工程约4秒的对齐时间
    BSP_DebugUart_TryWrite("Current Angle : Rotor Angle : Raw Encoder\r\n");

    for (int i = 0; i < n; i++)                         //正向扫描一个机械圈
    {
        for (int j = 0; j < n2; j++)
        {
            theta_ref += delta;
            abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);
            svm(1.0f, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);
            if (parameters->PHASE_ORDER)
            {
                PWM_SetCompare(0.5f * (1.0f + dtc_u),
                               0.5f * (1.0f + dtc_v),
                               0.5f * (1.0f + dtc_w));
            }
            else
            {
                PWM_SetCompare(0.5f * (1.0f + dtc_u),
                               0.5f * (1.0f + dtc_w),
                               0.5f * (1.0f + dtc_v));
            }
            wait_us(100U);
        }

        primask = __get_PRIMASK();
        __disable_irq();
        theta_actual = ps->position;                    //累计机械角度和原始计数取自同一次中断采样
        raw_f[i] = ps->raw;
        __set_PRIMASK(primask);
        error_f[i] = theta_ref / NPP - theta_actual;
        snprintf(text, sizeof(text), "%.4f\t,%.4f\t,%ld\r\n",
                 (double)(theta_ref / NPP), (double)theta_actual, (long)raw_f[i]);
        BSP_DebugUart_TryWrite(text);
    }

    for (int i = 0; i < n; i++)                         //反向扫描，保留原工程的采样顺序
    {
        for (int j = 0; j < n2; j++)
        {
            theta_ref -= delta;
            abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);
            svm(1.0f, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);
            if (parameters->PHASE_ORDER)
            {
                PWM_SetCompare(0.5f * (1.0f + dtc_u),
                               0.5f * (1.0f + dtc_v),
                               0.5f * (1.0f + dtc_w));
            }
            else
            {
                PWM_SetCompare(0.5f * (1.0f + dtc_u),
                               0.5f * (1.0f + dtc_w),
                               0.5f * (1.0f + dtc_v));
            }
            wait_us(100U);
        }

        primask = __get_PRIMASK();
        __disable_irq();
        theta_actual = ps->position;
        raw_b[i] = ps->raw;
        __set_PRIMASK(primask);
        error_b[i] = theta_ref / NPP - theta_actual;
        snprintf(text, sizeof(text), "%.4f\t,%.4f\t,%ld\r\n",
                 (double)(theta_ref / NPP), (double)theta_actual, (long)raw_b[i]);
        BSP_DebugUart_TryWrite(text);
    }

    for (int i = 0; i < n; i++)
    {
        offset += (error_f[i] + error_b[n - 1 - i]) / (2.0f * n);
        error[i] = 0.5f * (error_f[i] + error_b[n - 1 - i]); //沿用原工程正反向配对公式
    }
    offset = fmodf(offset * NPP, 2.0f * PI);             //将平均机械角度误差转换为电角度偏置

    for (int i = 0; i < n; i++)
    {
        error_filt[i] = 0.0f;                            //每次计算前清零，支持重复执行标定
        for (int j = 0; j < window; j++)
        {
            int ind = -window / 2 + j + i;
            if (ind < 0)
            {
                ind += n;
            }
            else if (ind > n - 1)
            {
                ind -= n;
            }
            error_filt[i] += error[ind] / (float)window;
        }
        mean += error_filt[i] / n;
    }
    raw_offset = (raw_f[0] + raw_b[n - 1]) / 2;

    BSP_DebugUart_TryWrite("\r\nEncoder non-linearity compensation table\r\n");
    wait_us(1000U);
    for (int i = 0; i < n_lut; i++)
    {
        int ind = (raw_offset >> 7) + i;
        if (ind > n_lut - 1)
        {
            ind -= n_lut;
        }
        lut[ind] = (int32_t)((error_filt[i * NPP] - mean) * 16384.0f / (2.0f * PI)); //沿用14位编码器和原工程LUT公式
        snprintf(text, sizeof(text), "%d\t%d\t%ld\r\n", i, ind, (long)lut[ind]);
        BSP_DebugUart_TryWrite(text);
        wait_us(1000U);
    }

    memcpy(parameters->ENCODER_LUT, lut, sizeof(lut));   //更新原有参数结构体中的补偿表
    primask = __get_PRIMASK();
    __disable_irq();
    parameters->E_OFFSET = offset;
    PositionSensor_WriteLUT(lut);                        //使新的电角度偏置和LUT一起用于后续采样
    __set_PRIMASK(primask);
    snprintf(text, sizeof(text), "\r\nEncoder Electrical Offset (rad) %f\r\n", (double)offset);
    BSP_DebugUart_TryWrite(text);
}
