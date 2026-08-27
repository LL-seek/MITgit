                  #define REST_MODE 0
#define CALIBRATION_MODE 1
#define MOTOR_MODE 2
#define SETUP_MODE 4
#define ENCODER_MODE 5

#define VERSION_NUM "1.9"


float __float_reg[64];   //浮点参数RAM数组：启动时从Flash加载，修改后保存回Sector 6
int __int_reg[256];      //整数参数RAM数组：包含CAN参数、校准参数和编码器LUT

#include "mbed.h"
#include "PositionSensor.h"
#include "structs.h"
#include "foc.h"
#include "calibration.h"
#include "hw_setup.h"
#include "math_ops.h" 
#include "current_controller_config.h"
#include "hw_config.h"
#include "motor_config.h"
#include "stm32f4xx_flash.h"
#include "FlashWriter.h"
#include "user_config.h"
#include "PreferenceWriter.h"
#include "CAN_com.h"
#include "DRV.h"
 
PreferenceWriter prefs(6);        //创建参数读写对象，指定使用内部Flash的Sector 6保存和加载参数
                                  //（这个sector6是一块存储的位置）

GPIOStruct gpio;
ControllerStruct controller;
ObserverStruct observer;
COMStruct com;
Serial pc(PA_2, PA_3);


CAN          can(PB_8, PB_9, 1000000);      //CAN硬件初始化 使用PB8/PB9作为CAN1收发引脚，通信速率为1Mbps
CANMessage   rxMsg;                         //CAN接收帧缓存
CANMessage   txMsg;                         //CAN发送帧缓存

SPI drv_spi(PA_7, PA_6, PA_5);   
DigitalOut drv_cs(PA_4);        
DRV832x drv(&drv_spi, &drv_cs);  

PositionSensorAM5147 spi(16384, 0.0, NPP);  //PositionSensorAM5147(int CPR, float offset, int ppairs);

volatile int count = 0;
volatile int state = REST_MODE;
volatile int state_change;
volatile int DualEncoder=1;
volatile int can_zero_request = 0;
//2026.6.5添加
volatile int can_id_save_pending = 0;
volatile int can_master_save_pending = 0;

void onMsgReceived() {                          //CAN中断回调函数
    can.read(rxMsg);                            //从CAN接收FIFO里取出一帧，放到rxMsg。
    printf("%df\n\r", rxMsg.id);                //打印
    if((rxMsg.id == CAN_ID)){                   //上位机发送CAN帧->CAN收发器收到总线信号->STM32 CAN1外设接收到一帧->CAN过滤器判断：ID是否等于 CAN_ID->匹配成功，放进 CAN RX FIFO0->触发 CAN1_RX0 中断->进入接收回调函数
        controller.timeout = 0;  
        if(((rxMsg.data[0]==0xFF) & (rxMsg.data[1]==0xFF) & (rxMsg.data[2]==0xFF) & (rxMsg.data[3]==0xFF) & (rxMsg.data[4]==0xFF) & (rxMsg.data[5]==0xFF) & (rxMsg.data[6]==0xFF) & (rxMsg.data[7]==0xFC))){
            state = MOTOR_MODE;         
            }
        else if(((rxMsg.data[0]==0xFF) & (rxMsg.data[1]==0xFF) & (rxMsg.data[2]==0xFF) & (rxMsg.data[3]==0xFF) * (rxMsg.data[4]==0xFF) & (rxMsg.data[5]==0xFF) & (rxMsg.data[6]==0xFF) & (rxMsg.data[7]==0xFD))){
            state = REST_MODE;
            state_change = 1;
            gpio.led->write(0);
            }
        else if(((rxMsg.data[0]==0xFF) & (rxMsg.data[1]==0xFF) & (rxMsg.data[2]==0xFF) & (rxMsg.data[3]==0xFF) * (rxMsg.data[4]==0xFF) & (rxMsg.data[5]==0xFF) & (rxMsg.data[6]==0xFF) & (rxMsg.data[7]==0xFE))){
            can_zero_request = 1;
            }
				//2026.6.5 添加
				else if(((rxMsg.data[0]==0xFF) & (rxMsg.data[1]==0xFF) & (rxMsg.data[2]==0xFF) & (rxMsg.data[3]==0xFF) & (rxMsg.data[4]==0xFF) & (rxMsg.data[5]==0xFF) & (rxMsg.data[7]==0xFA))){
            int new_id = rxMsg.data[6];

            if((new_id >= 0) && (new_id <= 127)){
                CAN_ID = new_id;
                can.filter(CAN_ID, 0x7FF, CANStandard, 0);
                can_id_save_pending = 1;
            }
            }
				else if(((rxMsg.data[0]==0xFF) & (rxMsg.data[1]==0xFF) & (rxMsg.data[2]==0xFF) & (rxMsg.data[3]==0xFF) & (rxMsg.data[4]==0xFF) & (rxMsg.data[5]==0xFF) & (rxMsg.data[7]==0x01))){
           int new_master_id = rxMsg.data[6];

           if((new_master_id >= 0) && (new_master_id <= 0xFF)){
               CAN_MASTER = new_master_id;
               can_master_save_pending = 1;
          }
        }
		   //	
        else if(state == MOTOR_MODE){
            unpack_cmd(rxMsg, &controller);  
            }
        pack_reply(&txMsg, controller.theta_mech, controller.dtheta_mech, controller.i_q_filt*KT_OUT);			
        can.write(txMsg);
        }
    
}

void enter_menu_state(void){
    drv.disable_gd();
    //gpio.enable->write(0);
    printf("\n\r\n\r\n\r");
    printf(" Commands:\n\r");
    wait_us(10);
    printf(" m - Motor Mode\n\r");
    wait_us(10);
    printf(" c - Calibrate Encoder\n\r");
    wait_us(10);
    printf(" s - Setup\n\r");
    wait_us(10);
    printf(" e - Display Encoder\n\r");
    wait_us(10);
    printf(" z - Set Zero Position\n\r");
    wait_us(10);
    printf(" esc - Exit to Menu\n\r");
    wait_us(10);
    state_change = 0;
    gpio.led->write(0);
    }

void enter_setup_state(void){
    printf("\n\r\n\r Configuration Options \n\r\n\n");
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %-2s\n\r\n\r", "prefix", "parameter", "min", "max", "current value");
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %.1f\n\r", "b", "Current Bandwidth (Hz)", "100", "2000", I_BW);
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %-5i\n\r", "i", "CAN ID", "0", "127", CAN_ID);
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %-5i\n\r", "m", "CAN Master ID", "0", "127", CAN_MASTER);
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %.1f\n\r", "l", "Current Limit (A)", "0.0", "12.0", I_MAX);
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %.1f\n\r", "f", "FW Current Limit (A)", "0.0", "12.0", I_FW_MAX);
    wait_us(10);
    printf(" %-4s %-31s %-5s %-6s %d\n\r", "t", "CAN Timeout (cycles)(0 = none)", "0", "100000", CAN_TIMEOUT);
    wait_us(10);
    printf("\n\r To change a value, type 'prefix''value''ENTER'\n\r i.e. 'b1000''ENTER'\n\r\n\r");
    wait_us(10);
	
    //printf("电流环参数:\n");
		wait_us(10);
    printf("K_D = %.6f\n\r", controller.k_d);
		wait_us(10);
    printf("K_Q = %.6f\n\r", controller.k_q);
		wait_us(10);
    printf("KI_D = %.6f\n\r", controller.ki_d);
		wait_us(10);
    printf("KI_Q = %.6f\n\r", controller.ki_q);
		wait_us(10);
    
    //printf("系统参数:\n\r");
		wait_us(10);
    printf("I_SCALE = %.6f A/count\n\r", I_SCALE);
		wait_us(10);
    printf("V_BUS = %.1f V\n\r", controller.v_bus);
		wait_us(10);
    printf("DT = %.6f s\n\r", DT);	
		wait_us(10);
	
	zero_current(&controller.adc1_offset, &controller.adc2_offset);             // Measure current sensor zero-offset
    printf(" ADC1 Offset: %d    ADC2 Offset: %d\n\r", controller.adc1_offset, controller.adc2_offset);
		wait_us(10);
    printf(" Position Sensor Electrical Offset:   %.4f\n\r", E_OFFSET);
		wait_us(10);
    printf(" Output Zero Position:  %.4f\n\r", M_OFFSET);	
		wait_us(10);
    printf(" RAW_OFFSET:%d  RAW_OFFSET: %d\n\r", RAW_OFFSET,RAW2_OFFSET);		

		wait_us(10);
    printf(" DualEncoder_rotations:%d\n\r", spi.DualEncoderRotations());	
		wait_us(10);
    state_change = 0;
    }
    
void enter_torque_mode(void){
    drv.enable_gd();   
    controller.ovp_flag = 0;  
    reset_foc(&controller);                                                    
    wait(.001);
    controller.i_d_ref = 0;
    controller.i_q_ref = 0;                                                   
    gpio.led->write(1);                                                   
    state_change = 0;
    printf("\n\r Entering Motor Mode \n\r");
    }
    
void calibrate(void){
    drv.enable_gd();
    gpio.led->write(1);   
    order_phases(&spi, &gpio, &controller, &prefs);                             
    calibrate(&spi, &gpio, &controller, &prefs);                                
    gpio.led->write(0);                                                    
    wait(.2);
    printf("\n\r Calibration complete.  Press 'esc' to return to menu\n\r");
    drv.disable_gd();
     state_change = 0;
    }
    
void print_encoder(void){
    //printf(" Mechanical Angle:  %f    Electrical Angle:  %f    Raw:  %d    Raw2:  %d\n\r", spi.GetMechPosition(), spi.GetElecPosition(), spi.GetRawPosition(), spi.GetRaw2Position());
    printf("Mechanical Angle_GR:%f\n\r", spi.GetMechPosition_GR());
		printf("theta_mech:%f\n\r", controller.theta_mech);
    wait(.5);
    }


extern "C" void TIM1_UP_TIM10_IRQHandler(void) {
  if (TIM1->SR & TIM_SR_UIF ) {

        ADC1->CR2  |= 0x40000000;                                               // Begin sample and conversion

        spi.Sample(DT);                                                           // sample position sensor
        controller.adc2_raw = ADC2->DR;                                         // Read ADC Data Registers
        controller.adc1_raw = ADC1->DR;	

		
        controller.adc3_raw = ADC3->DR;

        controller.theta_elec = spi.GetElecPosition();

				controller.theta_mech = spi.GetMechPosition_GR();
		
        controller.dtheta_mech = (DUAL_ENCODER_DIR/GR)*spi.GetMechVelocity(); 				

        controller.dtheta_elec = spi.GetElecVelocity();
        controller.v_bus = 0.95f*controller.v_bus + 0.05f*((float)controller.adc3_raw)*V_SCALE; //filter the dc link voltage measurement
        ///
        
        /// Check state machine state, and run the appropriate function ///
        switch(state){
            case REST_MODE:                                                     // Do nothing
                if(state_change){
                    enter_menu_state();

                    } 
                break;
            
            case CALIBRATION_MODE:                                              // Run encoder calibration procedure
                if(state_change){
                    calibrate();
                    }
                break;
             
            case MOTOR_MODE:                                                   // Run torque control
                if(state_change){
                    enter_torque_mode();
                    count = 0;
									
										if(DualEncoder)
										{
											int i=1;
											for(int i=0;i<10;i++)
											{
												spi.DualEncoder();
												wait_ms(100);
											}
											DualEncoder=0;	
										}					
                    }
                else{


                if((controller.timeout > CAN_TIMEOUT) && (CAN_TIMEOUT > 0)){

                    controller.i_d_ref = 0;
                    controller.i_q_ref = 0;
                    controller.kp = 0;
                    controller.kd = 0;
                    controller.t_ff = 0;
                   }

                torque_control(&controller);
                commutate(&controller, &observer, &gpio, controller.theta_elec);           // Run current loop

                controller.timeout++;
                count++; 
            
                }     
                break;
            case SETUP_MODE:
                if(state_change){
                    enter_setup_state();
                }
                break;
            case ENCODER_MODE:
                print_encoder();
                break;
                }                 
      }
  TIM1->SR = 0x0;                                                               // reset the status register
}


char cmd_val[8] = {0};
char cmd_id = 0;
char char_count = 0;

void sample_position_sensor_for_zero(void){
    spi.Sample(DT);
    wait_us(20);
    spi.Sample(DT);
    wait_us(20);
    spi.Sample(DT);
}

void save_zero_position(bool print_status){
    spi.SetMechOffset(0);
    NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
    sample_position_sensor_for_zero();

    RAW_OFFSET = spi.GetRawPosition();
    RAW2_OFFSET = spi.GetRaw2Position();
    NVIC_ClearPendingIRQ(TIM1_UP_TIM10_IRQn);
    NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
    M_OFFSET = ((float)RAW_OFFSET/(float)spi.GetCPR())*2.0f*PI;

    if(!prefs.ready()){     //如果参数写入器未打开
    prefs.open();           //解锁Flash、清除旧状态并擦除参数扇区
    }
    prefs.flush();          //将RAM中的全部整数和浮点参数写入Flash
    prefs.close();          //锁定Flash，结束本次写入
    prefs.load();           //重新从Flash读取参数到RAM数组

    spi.SetMechOffset(M_OFFSET);
    spi.SetRAWOffset(RAW_OFFSET);
    spi.SetRAW2Offset(RAW2_OFFSET);

    NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
    spi.ResetRotationState();
    sample_position_sensor_for_zero();
    NVIC_ClearPendingIRQ(TIM1_UP_TIM10_IRQn);
    NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

    if(print_status){
        printf("\n\r  Saved new zero position:  %.4f\n\r\n\r", M_OFFSET);
        printf("\n\r  Saved RAW_OFFSET:  %d\n\r\n\r", RAW_OFFSET);
        printf("\n\r  Saved RAW2_OFFSET:  %d\n\r\n\r", RAW2_OFFSET);
    }

    NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
    spi.DualEncoder();
    NVIC_ClearPendingIRQ(TIM1_UP_TIM10_IRQn);
    NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

void serial_interrupt(void){
    while(pc.readable()){
        char c = pc.getc();
        if(c == 27){            //Esc
                state = REST_MODE;
                state_change = 1;
                char_count = 0;
                cmd_id = 0;
                gpio.led->write(0);; 
                for(int i = 0; i<8; i++){cmd_val[i] = 0;}
                }
        if(state == REST_MODE){
            switch (c){
                case 'c':
                    state = CALIBRATION_MODE;
                    state_change = 1;
                    break;
                case 'm':
                    state = MOTOR_MODE;
                    state_change = 1;
                    break;
                case 'e':
                    state = ENCODER_MODE;
                    state_change = 1;
                    break;
                case 's':
                    state = SETUP_MODE;
                    state_change = 1;
                    break;
                case 'z':
                    can_zero_request = 1;
                    break;
                }
                
                }
        else if(state == SETUP_MODE){
            if(c == 13){                        //CR
                switch (cmd_id){
                    case 'b':
                        I_BW = fmaxf(fminf(atof(cmd_val), 2000.0f), 100.0f);
                        break;
                    case 'i':
                        CAN_ID = atoi(cmd_val);
                        break;
                    case 'm':
                        CAN_MASTER = atoi(cmd_val);
                        break;
                    case 'l':
                        I_MAX = fmaxf(fminf(atof(cmd_val), I_MAX_MOTOR), 0.0f);
                        break;
                    case 'f':
                        I_FW_MAX = fmaxf(fminf(atof(cmd_val), I_MAX_MOTOR), 0.0f);
                        break;
                    case 't':
                        CAN_TIMEOUT = atoi(cmd_val);
                        break;
                    default:
                        printf("\n\r '%c' Not a valid command prefix\n\r\n\r", cmd_id);
                        break;
                    }
                    
                if (!prefs.ready()) prefs.open();
                prefs.flush();                                                  // Write new prefs to flash
                prefs.close();    
                prefs.load();                                              
                state_change = 1;
                char_count = 0;
                cmd_id = 0;
                for(int i = 0; i<8; i++){cmd_val[i] = 0;}
                }
            else{
                if(char_count == 0){cmd_id = c;}
                else{
                    cmd_val[char_count-1] = c;
                    
                }
                pc.putc(c);
                char_count++;
                }
            }
        else if (state == ENCODER_MODE){
            switch (c){
                case 27:
                    state = REST_MODE;
                    state_change = 1;
                    break;
                    }
            }
        else if (state == MOTOR_MODE){
            switch (c){
                case 'd':
                    controller.i_q_ref = 0;
                    controller.i_d_ref = 0;
                }
            }
            
        }
    }
       
int main() {
    controller.v_bus = V_BUS;
    controller.mode = 0;
		wait(.100);
    Init_All_HW(&gpio);                                                         // Setup PWM, ADC, GPIO
    wait(.1);
    
    gpio.enable->write(1);
    wait_us(100);
    drv.calibrate();
    wait_us(100);
	//  3x PWM mode；clear latched fault bits.This bit automatically resets after being writen.
    drv.write_DCR(0x0, 0x0, 0x0, PWM_MODE_3X, 0x0, 0x0, 0x0, 0x0, 0x1);
    wait_us(100);
	//Sense amplifier reference voltage is VREF divided by 2；40-V/V shunt amplifier gain；Sense OCP 1 V
    drv.write_CSACR(0x0, 0x1, 0x0, CSA_GAIN_40, 0x0, 0x0, 0x0, 0x0, SEN_LVL_1_0);
		//drv.write_CSACR(0x0, 0x1, 0x0, CSA_GAIN_40, 0x0, 0x0, 0x0, 0x0, SEN_LVL_0_25);  //初始电流限制为25A，由于电源最大电流为5A，容易造成电压电压拉低后造成芯片损坏，所以将限流改为6.25A(目前最小)

    wait_us(100);
	//VDS_OCP and SEN_OCP retry time is 4 ms；200-ns dead time；Overcurrent causes an automatic retrying fault；
	//Overcurrent deglitch of 8 us；1.88 V
    drv.write_OCPCR(TRETRY_4MS, DEADTIME_200NS, OCP_RETRY, OCP_DEG_8US, VDS_LVL_1_88);
    
    //drv.enable_gd();
    //zero_current(&controller.adc1_offset, &controller.adc2_offset);             // Measure current sensor zero-offset
    drv.disable_gd();   //put all MOSFETs in the Hi-Z state

    wait(.1);
    reset_foc(&controller);                                                     // Reset current controller
    reset_observer(&observer);                                                 // Reset observer
    TIM1->CR1 ^= TIM_CR1_UDIS;                                                  //Update disable

    
    wait(.1);
    NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 2);                  
    
    NVIC_SetPriority(CAN1_RX0_IRQn, 3);                   NVIC_SetPriority(CAN1_RX0_IRQn, 3);   //设置CAN1接收FIFO0中断优先级为3
    
   
    prefs.load();                                                //读Flash参数，若参数无效，下面逐项恢复默认
		//浮点参数：Flash内容为NaN时，使用默认值
    if(isnan(E_OFFSET)){E_OFFSET = 0.0f;}                        //编码器电角度偏移默认值
    if(isnan(M_OFFSET)){M_OFFSET = 0.0f;}                        //编码器机械零位偏移默认值
    if(isnan(I_BW) || I_BW==-1||I_BW==0){I_BW = 1000;}           //电流环带宽默认值
    if(isnan(I_MAX) || I_MAX ==-1||I_MAX==0){I_MAX=I_MAX_MOTOR;} //最大电流无效时，使用电机允许的最大值
    I_MAX = fmaxf(fminf(I_MAX, I_MAX_MOTOR), 0.0f);              //将最大电流限制在 0.0f ～ I_MAX_MOTOR 范围内
    if(isnan(I_FW_MAX) || I_FW_MAX ==-1){I_FW_MAX=0;}            //弱磁最大电流无效时，默认关闭弱磁
    I_FW_MAX = fmaxf(fminf(I_FW_MAX, I_MAX_MOTOR), 0.0f);        //将弱磁最大电流限制在 0.0f ～ I_MAX_MOTOR 范围内
    if(isnan(CAN_ID) || CAN_ID==-1){CAN_ID = 1;}                 //如果Flash中没有有效的本机CAN接收ID，则默认本机ID为1
    if(isnan(CAN_MASTER) || CAN_MASTER==-1){CAN_MASTER = 0;}     //如果Flash中没有有效的主机CAN ID，则默认回复/主机ID为0 
    if(isnan(CAN_TIMEOUT) || CAN_TIMEOUT==-1){CAN_TIMEOUT = 0;}  //如果Flash中没有有效的CAN超时参数，则默认关闭/不启用CAN超时保护     
		
		//2026年1月9日 新添加		 
    if(isnan(RAW_OFFSET)){RAW_OFFSET = 0;}                       //主编码器原始零位默认值   
    if(isnan(RAW2_OFFSET)){RAW2_OFFSET = 0;} 		                 //副编码器原始零位默认值
		
    spi.SetElecOffset(E_OFFSET);                                              // Set position sensor offset
    spi.SetMechOffset(M_OFFSET); 
		//2026年1月9日 新添加
    spi.SetRAWOffset(RAW_OFFSET);                                             // Set position sensor offset
    spi.SetRAW2Offset(RAW2_OFFSET);		
		
//		//2026年1月14日 新添加
//		spi.DualEncoder();
		
    int lut[128] = {0};
    memcpy(lut, &ENCODER_LUT, 128*4);
    spi.WriteLUT(lut);                                                          // Set potision sensor nonlinearity lookup table
    init_controller_params(&controller);

		
		zero_current(&controller.adc1_offset, &controller.adc2_offset);             // Measure current sensor zero-offset
		
    pc.baud(921600);                                                            // set serial baud rate
    wait(.01);
    pc.printf("\n\r\n\r 5636 Motor\n\r\n\r");
    wait(.01);
    printf("\n\r Debug Info:\n\r");
    printf(" Firmware Version: %s\n\r", VERSION_NUM);
    printf(" ADC1 Offset: %d    ADC2 Offset: %d\n\r", controller.adc1_offset, controller.adc2_offset);
    printf(" Position Sensor Electrical Offset:   %.4f\n\r", E_OFFSET);
    printf(" Output Zero Position:  %.4f\n\r", M_OFFSET);
    printf(" CAN ID:  %d\n\r", CAN_ID);
//2026.5.13添加
		printf("\n\r  Saved RAW_OFFSET:  %d\n\r", RAW_OFFSET);
		printf("\n\r  Saved RAW2_OFFSET:  %d\n\r", RAW2_OFFSET);   
    //printf(" DualEncoder_rotations:%d\n\r", spi.DualEncoderRotations());	

//spi.DualEncoder();


    can.filter(CAN_ID, 0x7FF, CANStandard, 0);   //配置CAN接收过滤器，只接收标准帧ID等于CAN_ID的消息

    txMsg.id = CAN_MASTER;   //设置CAN回复帧ID为主机/上位机ID，表示反馈消息发给CAN_MASTER
    txMsg.len = 6;           //设置CAN回复帧数据长度为6字节，对应pack_reply()打包的位置/速度/力矩反馈
    rxMsg.len = 8;           //设置CAN接收帧期望数据长度为8字节，对应unpack_cmd()解析的位置/速度/Kp/Kd/前馈力矩命令
    can.attach(&onMsgReceived);                  //绑定CAN接收中断回调，收到CAN消息后自动执行onMsgReceived(),触发回调函数

    pc.attach(&serial_interrupt);                                              
    
    state_change = 1;

    while(1) {
       if(can_zero_request){
           can_zero_request = 0;
           save_zero_position(true);
       }
			 //2026.6.5添加
			if(can_id_save_pending){
         can_id_save_pending = 0;

         if (!prefs.ready()) prefs.open();
         prefs.flush();
         prefs.close();
         prefs.load();
      }
			if(can_master_save_pending){
        can_master_save_pending = 0;

         if (!prefs.ready()) prefs.open();
         prefs.flush();
         prefs.close();
         prefs.load();
      }
			//
       drv.print_faults();
       wait(.1);		
						

    }
}
