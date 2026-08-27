/// Calibration procedures for determining position sensor offset, 
/// phase ordering, and position sensor linearization
/// 

#include "calibration.h"
#include "foc.h"
#include "PreferenceWriter.h"
#include "user_config.h"
#include "motor_config.h"
#include "current_controller_config.h"
#include "DRV.h"
extern DRV832x drv;

void order_phases(PositionSensor *ps, GPIOStruct *gpio, ControllerStruct *controller, PreferenceWriter *prefs){   
    
    printf("\n\r Checking phase ordering\n\r");
    float theta_ref = 0;                           //程序希望产生的定子磁场电角度
    float theta_actual = 0;                        //编码器测得的实际机械角度
    float v_d = V_CAL;                             //给定的d轴电压（用来建立一个定子磁场，让转子与这个磁场对齐）                                                          
    float v_q = 0.0f;                              //给定的q轴电压（没有主动命令转矩）
    float v_u, v_v, v_w = 0;                       //三相期望电压（都为1半，没有电压差，不会形成旋转磁场）
    float dtc_u, dtc_v, dtc_w = .5f;               //三相PWM占空比
    int sample_counter = 0;
    
    abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);                   //反park变换（输入参考电角度d轴电压q轴电压，输出uvw三相电压）
    svm(1.0, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);              //空间矢量调制（把三相电压转成占空比）

    for(int i = 0; i<20000; i++){                                 //保持固定磁场两秒（两秒是等待100us*20000得到的）
        TIM1->CCR3 = (PWM_ARR>>1)*(1.0f-dtc_u);                                        
        TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_v);
        TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_w);
        wait_us(100);
        }
    
    ps->Sample(DT);                               //读取编码器（但是并没有保存）
    wait_us(1000);
   
    float theta_start;                            //声明变量
    controller->i_b = I_SCALE*(float)(controller->adc2_raw - controller->adc2_offset);    //计算B相电流（（ADC采集到的电流原始值 - ADC偏置）* 实际电流值）
    controller->i_c = I_SCALE*(float)(controller->adc1_raw - controller->adc1_offset);
    controller->i_a = -controller->i_b - controller->i_c;                                 //根据三相电流之和计算A相电流
    dq0(controller->theta_elec, controller->i_a, controller->i_b, controller->i_c, &controller->i_d, &controller->i_q);    //Clark变换和park变换
    float current = sqrt(pow(controller->i_d, 2) + pow(controller->i_q, 2));              //根据勾股定理计算电流矢量大小
    printf("\n\rCurrent\n\r");
    printf("%f\t    %f\t   %f\r\n", controller->i_d, controller->i_q, current);

    while(theta_ref < 4*PI){                                                              //两个电周期
        abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);                            
        svm(1.0, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);                        
        wait_us(100);
        TIM1->CCR3 = (PWM_ARR>>1)*(1.0f-dtc_u);                                        
        TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_v);
        TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_w);
			
       ps->Sample(DT);                                                            
       theta_actual = ps->GetMechPositionFixed();                                         //获取编码器测得的实际机械角度
       if(theta_ref==0){theta_start = theta_actual;}                                      //记录第一次转子的其实机械角度
       if(sample_counter > 200){                                                          //200个循环打印一次
           sample_counter = 0 ;
        printf("%.4f\t   %.4f\r\n", theta_ref/(NPP), theta_actual);
        }
        sample_counter++;
       theta_ref += 0.001f;                                                              //每次将参考电角度增加0.001弧度，使磁场缓慢向前旋转
        }
    float theta_end = ps->GetMechPositionFixed();                                        //测得最终的机械角度
    int direction = (theta_end - theta_start)>0;                                         //比较初始和最终的机械角度
    printf("Theta Start:   %f    Theta End:  %f\n\r", theta_start, theta_end);
    printf("Direction:  %d\n\r", direction);
    if(direction){printf("Phasing correct\n\r");}                                        //根据方向判断相序是否正确
    else if(!direction){printf("Phasing incorrect.  Swapping phases V and W\n\r");}
    PHASE_ORDER = direction;
    }
    
    
void calibrate(PositionSensor *ps, GPIOStruct *gpio, ControllerStruct *controller, PreferenceWriter *prefs){      //标定主编码器

    printf("Starting calibration procedure\n\r");
    float *error_f;          //正转位置误差
    float *error_b;          //反转位置误差
    float *error;            //正反转平均误差
    float *error_filt;       //滤波后的非线性误差
    int *raw_f;              //正转原始编码器值
    int *raw_b;              //反转原始编码器值
    int *lut;                //128 点编码器补偿表
    
    const int n = 128*NPP;                       //每机械圈采集128*NPP个点     
    const int n2 = 40;                           //两个保存点之间细分40个个小步
    float delta = 2*PI*NPP/(n*n2);               //每个小步增加的电角度，全部执行刚好一个机械圈
    error_f = new float[n]();                    //括号 () 表示分配后清零       
    error_b = new float[n]();                            
    const int  n_lut = 128;
    lut = new int[n_lut]();                             
    
    error = new float[n]();
    const int window = 128;                       //128个采样点正好覆盖当前电机的一个电周期
    error_filt = new float[n]();
    float cogging_current[window] = {0};
    
    ps->WriteLUT(lut);                            //先向位置传感器写入全零 LUT，防止原有补偿表干扰本次原始误差测量。
    raw_f = new int[n]();
    raw_b = new int[n]();
		
    float theta_ref = 0;                           //程序希望产生的定子磁场电角度
    float theta_actual = 0;                        //编码器测得的实际机械角度
    float v_d = V_CAL;                             //给定的d轴电压（用来建立一个定子磁场，让转子与这个磁场对齐）                                                          
    float v_q = 0.0f;                              //给定的q轴电压（没有主动命令转矩）
    float v_u, v_v, v_w = 0;                       //三相期望电压（都为1半，没有电压差，不会形成旋转磁场）
    float dtc_u, dtc_v, dtc_w = .5f;               //三相PWM占空比
    
        
    abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);                   //反park变换（输入参考电角度d轴电压q轴电压，输出uvw三相电压）
    svm(1.0, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);              //空间矢量调制（把三相电压转成占空比）
		
    for(int i = 0; i<40000; i++){                                 //设立循环，大约4秒
        TIM1->CCR3 = (PWM_ARR>>1)*(1.0f-dtc_u);                                       
        if(PHASE_ORDER){                                   
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_v);
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
        else{
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_v);
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
        wait_us(100);
        }

    ps->Sample(DT);   
    controller->i_b = I_SCALE*(float)(controller->adc2_raw - controller->adc2_offset);    //计算B相电流（（ADC采集到的电流原始值 - ADC偏置）* 实际电流值）
    controller->i_c = I_SCALE*(float)(controller->adc1_raw - controller->adc1_offset);
    controller->i_a = -controller->i_b - controller->i_c;                                 //根据三相电流之和计算A相电流
    dq0(controller->theta_elec, controller->i_a, controller->i_b, controller->i_c, &controller->i_d, &controller->i_q);    //Clark变换和park变换
    float current = sqrt(pow(controller->i_d, 2) + pow(controller->i_q, 2));              //根据勾股定理计算电流矢量大小
    printf(" Current Angle : Rotor Angle : Raw Encoder \n\r\n\r");

    for(int i = 0; i<n; i++){                                    //正向旋转一机械圈，共记录 n 个标定点
       for(int j = 0; j<n2; j++){                                //每个标定点分成 n2 个小步，使转子平滑跟随旋转磁场
				 
        theta_ref += delta;                                      //更新指令电角度，并转换成三相电压和PWM占空比
        abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);                              
        svm(1.0, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);                        
				 
        TIM1->CCR3 = (PWM_ARR>>1)*(1.0f-dtc_u);                  //输出U相PWM；根据相序决定V、W是否交换
        if(PHASE_ORDER){                                                       
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_v);                                  
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
        else{
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_v);
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
				
            wait_us(100);                                       //等待转子跟随，并更新编码器位置
            ps->Sample(DT);
        }
			 
       ps->Sample(DT);
       theta_actual = ps->GetMechPositionFixed();
       error_f[i] = theta_ref/NPP - theta_actual;
       raw_f[i] = ps->GetRawPosition();
       printf("%.4f\t,%.4f\t,%d\r\n", theta_ref/(NPP), theta_actual, raw_f[i]);

        }
    
    for(int i = 0; i<n; i++){                                                   // rotate backwards
       for(int j = 0; j<n2; j++){
       theta_ref -= delta;
       abc(theta_ref, v_d, v_q, &v_u, &v_v, &v_w);                              // inverse dq0 transform on voltages
       svm(1.0, v_u, v_v, v_w, &dtc_u, &dtc_v, &dtc_w);                         // space vector modulation
        TIM1->CCR3 = (PWM_ARR>>1)*(1.0f-dtc_u);
        if(PHASE_ORDER){
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_v);
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
        else{
            TIM1->CCR1 = (PWM_ARR>>1)*(1.0f-dtc_v);
            TIM1->CCR2 = (PWM_ARR>>1)*(1.0f-dtc_w);
            }
            wait_us(100);
            ps->Sample(DT);
        }
       ps->Sample(DT);                                                            // sample position sensor
       theta_actual = ps->GetMechPositionFixed();                                    // get mechanical position
       error_b[i] = theta_ref/NPP - theta_actual;
       raw_b[i] = ps->GetRawPosition();
       //printf("%.4f\t   %.4f\t    %d\r\n", theta_ref/(NPP), theta_actual, raw_b[i]);
				printf("%.4f\t,%.4f\t,%d\r\n", theta_ref/(NPP), theta_actual, raw_b[i]);
       //theta_ref -= delta;
        }    
        
        float offset = 0;                                  
        for(int i = 0; i<n; i++){
            offset += (error_f[i] + error_b[n-1-i])/(2.0f*n);                   // calclate average position sensor offset
            }
        offset = fmod(offset*NPP, 2*PI);                                        // convert mechanical angle to electrical angle
        
        
        ps->SetElecOffset(offset);                                              // Set position sensor offset
        __float_reg[0] = offset;
        E_OFFSET = offset;
        
        
        float mean = 0;
        for (int i = 0; i<n; i++){                                              //Average the forward and back directions
            error[i] = 0.5f*(error_f[i] + error_b[n-i-1]);
            }
        for (int i = 0; i<n; i++){
            for(int j = 0; j<window; j++){
                int ind = -window/2 + j + i;                                    // Indexes from -window/2 to + window/2
                if(ind<0){
                    ind += n;}                                                  // Moving average wraps around
                else if(ind > n-1) {
                    ind -= n;}
                error_filt[i] += error[ind]/(float)window;
                }
            if(i<window){
                cogging_current[i] = current*sinf((error[i] - error_filt[i])*NPP);
                }

            mean += error_filt[i]/n;
            }
        int raw_offset = (raw_f[0] + raw_b[n-1])/2;                             //Insensitive to errors in this direction, so 2 points is plenty
        
        
        printf("\n\r Encoder non-linearity compensation table\r\n");
        printf(" Sample Number : Lookup Index : Lookup Value\n\r\n\r");
        for (int i = 0; i<n_lut; i++){                                          // build lookup table
            int ind = (raw_offset>>7) + i;
            if(ind > (n_lut-1)){ 
                ind -= n_lut;
                }
            lut[ind] = (int) ((error_filt[i*NPP] - mean)*(float)(ps->GetCPR())/(2.0f*PI));
            printf("%d\t   %d\t   %d \r\n", i, ind, lut[ind]);
            wait(.001);
            }
            
        ps->WriteLUT(lut);                                                      // write lookup table to position sensor object
        memcpy(&ENCODER_LUT, lut, 128*4);                                 // copy the lookup table to the flash array
        printf("\n\rEncoder Electrical Offset (rad) %f\r\n",  offset);
        
        if (!prefs->ready()) prefs->open();
        prefs->flush();                                                         // write offset and lookup table to flash
        prefs->close();
        
				delete[] error_filt;
				delete[] error;		
        delete[] error_f;       //gotta free up that ram
        delete[] error_b;
        delete[] lut;
        delete[] raw_f;
        delete[] raw_b;

    }
