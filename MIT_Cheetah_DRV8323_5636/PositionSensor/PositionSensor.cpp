
#include "mbed.h"
#include "PositionSensor.h"
#include "../math_ops.h"
#include "../Config/motor_config.h"
#include <iostream>
#include <cmath>
double round1(double value) {
    return value >= 0.0 ? floor(value + 0.5) : ceil(value - 0.5);
}
PositionSensorAM5147::PositionSensorAM5147(int CPR, float offset, int ppairs){
    _CPR = CPR;                     //编码器一圈的计数值
    _ppairs = ppairs;               //电机极对数
    ElecOffset = offset;            //电角度初始偏移量
    rotations = 0;                  //编码器累计跨圈数
	
	  old_counts = _CPR/2;            //上一次角度计数初值，放在半圈位置，避免刚启动时误判跨圈
	
    spi = new SPI(PC_12, PC_11, PC_10);       //创建 AS5047 编码器使用的 SPI 通信对象
    spi->format(16, 1);                       //配置 SPI 数据格式：每次传输 16 bit，SPI Mode 1
    spi->frequency(25000000);                 //配置 SPI 时钟频率为 25 MHz
    
		cs = new DigitalOut(PA_15);               //PA15 拉低时，选中主 AS5047
    cs->write(1);                             //PA15拉高
	
		cs2 = new DigitalOut(PD_2);               //PD2 拉低时，选中副 AS5047
		cs2->write(1);                            //PD2拉高
	
		readAngleCmd = 0x7FFE;                    //AS5047 读取角度寄存器的 16bit 命令
	
    MechOffset = offset;                      //机械角度偏移量初始化
    modPosition = 0;                          //一圈内机械角度
    oldModPosition = 0;                       //上一次一圈内机械角度
    oldVel = 0;                               //上一次速度，初始化为 0
    raw = 0;                                  //编码器原始角度计数，初始化为 0
    }

//双编码器程序 2026年1月5日	
void PositionSensorAM5147::DualEncoder() {
    GPIOA->ODR &= ~(1 << 15);          //PA15=0  CS 主编码器
		wait_us(2);

		raw = spi->write(readAngleCmd);		 //AS5047P
		raw = raw & 0x3FFF; 	             //取14位角度数据
    GPIOA->ODR |= (1 << 15);   	       //GPIO port output data register,PA15=1	
	
	
		GPIOD->ODR &= ~(1 << 2);           // PD2=0
		wait_us(2);

		raw2 = spi->write(readAngleCmd);	 //AS5047P
		raw2 = raw2 & 0x3FFF;              //取14位角度数据
		GPIOD->ODR |= (1 << 2);            //PD2=1		

		int e1,e2,diff;
		float K;
	
		e1 = raw - RawOffset;              //减去零位偏移
		e2 = raw2 - Raw2Offset;
		if(e1<0)                           
		{
			e1=e1+_CPR;
		}
		if(e2<0)
		{
			e2=e2+_CPR;
		}		
		diff = e1-e2;                  //编码器计数差值
		if(diff < -(_CPR/2))
		{
			diff = diff + _CPR;
		}
		if(diff > (_CPR/2))
		{
			diff = diff - _CPR;
		}		
		K=DUAL_ENCODER_DIR*(float)diff*GR/(float)_CPR;	

		MechPosition_GR=K*(2.0f*PI/GR);
		
		DualEncoder_rotations=(int)K;

		
		rotations=DualEncoder_rotations;
		
		printf("\n\r  raw:%d  raw2:%d\n\r", raw,raw2);
		printf("\n\r  e1:%d  e2:%d\n\r", e1,e2);
		printf(" K:  %.4f\n\r", K);
		printf(" DualEncoder_rotations:%d\n\r", DualEncoder_rotations);
		printf(" rotations:%d\n\r", rotations);
		printf(" MechPosition_GR: %.4f\n\r", MechPosition_GR);
		printf(" theta_mech: %.4f\n\r", MechPosition_GR);
		
}

    
void PositionSensorAM5147::Sample(float dt){
			

//削工	
    GPIOA->ODR &= ~(1 << 15);                       //拉低PA15
		raw= spi->write(readAngleCmd);                  //读取命令 
	  raw = raw & 0x3FFF;                             //取14位数据
		GPIOA->ODR |= (1 << 15);                        //拉高PA15
	
		GPIOD->ODR &= ~(1 << 2);                        //同上
		raw2=spi->write(readAngleCmd);
		raw2 = raw2 & 0x3FFF;
		GPIOD->ODR |= (1 << 2); 	
	
/*测试*/
		int e1,e2,diff;
		float K;
	
		e1 = raw - RawOffset;                           //减去零点位移7
		e2 = raw2 - Raw2Offset;
		if(e1<0)
		{
			e1=e1+_CPR;
		}
		if(e2<0)
		{
			e2=e2+_CPR;
		}		
		diff = e1-e2;                                  //编码器差值

		if(diff < -(_CPR/2))
		{
			diff = diff + _CPR;
		}
		if(diff > (_CPR/2))
		{
			diff = diff - _CPR;
		}
		
		K=DUAL_ENCODER_DIR*(float)diff*GR/(float)_CPR;	       //估算圈数

		MechPosition_GR=K*(2.0f*PI/GR);                        //输出轴机械位置
/*测试*/
	
    int off_1 = offset_lut[raw>>7];                        //线性插值
    int off_2 = offset_lut[((raw>>7)+1)%128];
    int off_interp = off_1 + ((off_2 - off_1)*(raw - ((raw>>7)<<7))>>7);      
    int angle = raw + off_interp;                          //修正后的计数值	
		
						
		
    if(angle - old_counts > _CPR/2){
        rotations -= 1;

        }
    else if (angle - old_counts < -_CPR/2){
        rotations += 1;

        }
    old_counts = angle;                                               //保存本次角度，供下次跨圈判断

    oldModPosition = modPosition;                                     //保存单圈机械角度
    modPosition = ((2.0f*PI * ((float) angle))/ (float)_CPR);         //当前单圈机械角度
    position = (2.0f*PI * ((float) angle+(_CPR*rotations)))/ (float)_CPR;              //多圈累计机械角度
    MechPosition = position - MechOffset;                                              //扣除机械零位偏移后的多圈累计机械角度
				
				
    float elec = ((2.0f*PI/(float)_CPR) * (float) ((_ppairs*angle)%_CPR)) + ElecOffset;	    //根据机械角度计算电角度，并加上电角度零位偏移
				
    if(elec < 0) elec += 2.0f*PI;
    else if(elec > 2.0f*PI) elec -= 2.0f*PI ; 
    ElecPosition = elec;                                              //保存FOC用到的电角度
 
 
    float vel;
    

    if((modPosition-oldModPosition) < -3.0f){                        //根据当前单圈机械角度和上一周期单圈机械角度计算瞬时机械角速度
        vel = (modPosition - oldModPosition + 2.0f*PI)/dt;
        }
    
    else if((modPosition - oldModPosition) > 3.0f){
        vel = (modPosition - oldModPosition - 2.0f*PI)/dt;
        }
    else{
        vel = (modPosition-oldModPosition)/dt;
    }    
    
    int n = 40;
    float sum = vel;                                      //先把当前最新的瞬时机械速度加入求和
    for (int i = 1; i < (n); i++){             
        velVec[n - i] = velVec[n-i-1];                    //速度缓存整体后移一位
        sum += velVec[n-i];                   
        }
    velVec[0] = vel;                                     //把当前最新速度放到速度缓存最前面
    MechVelocity =  sum/((float)n);                      //最近 40 次速度求平均，得到滤波后的机械角速度
    ElecVelocity = MechVelocity*_ppairs;                 //机械角速度乘以极对数，得到电角速度
    ElecVelocityFilt = 0.99f*ElecVelocityFilt + 0.01f*ElecVelocity;         //一阶低通滤波，进一步平滑电角速度
				
				
    }

int PositionSensorAM5147::GetRawPosition(){           //返回raw值
    return raw;
    }

float PositionSensorAM5147::GetMechPositionFixed(){
    return MechPosition+MechOffset;
    }
    
float PositionSensorAM5147::GetMechPosition(){
    return MechPosition;
    }
//2026年1月8日 添加
float PositionSensorAM5147::GetMechPosition_GR(){
    return MechPosition_GR;
    }
int PositionSensorAM5147::GetRaw2Position(){         //返回raw2值
    return raw2;
    }

int PositionSensorAM5147::DualEncoderRotations(){
    return DualEncoder_rotations;
    }		
		
float PositionSensorAM5147::GetElecPosition(){
    return ElecPosition;
    }

float PositionSensorAM5147::GetElecVelocity(){
    return ElecVelocity;
    }

float PositionSensorAM5147::GetMechVelocity(){
    return MechVelocity;
    }

void PositionSensorAM5147::ZeroPosition(){
    rotations = 0;
    MechOffset = 0;
    Sample(.00025f);
    MechOffset = GetMechPosition();
    }

void PositionSensorAM5147::SetElecOffset(float offset){
    ElecOffset = offset;
    }
void PositionSensorAM5147::SetMechOffset(float offset){
    MechOffset = offset;
    }

//2026年1月9日 新添加
void PositionSensorAM5147::SetRAWOffset(int offset){           //设置零位偏移
    RawOffset = offset;
    }
void PositionSensorAM5147::SetRAW2Offset(int offset){          //设置零位偏移
    Raw2Offset = offset;
    }		
		


void PositionSensorAM5147::ResetRotationState(){               //重置编码器
     rotations = 0;                                            
     DualEncoder_rotations = 0;
     old_counts = raw;
     position = 0.0f;
     MechPosition = 0.0f;
     MechPosition_GR = 0.0f;
    }
		
		
int PositionSensorAM5147::GetCPR(){                            //得到编码器总数
    return _CPR;
    }


void PositionSensorAM5147::WriteLUT(int new_lut[128]){
    memcpy(offset_lut, new_lut, 128*4);
    }
    


PositionSensorEncoder::PositionSensorEncoder(int CPR, float offset, int ppairs) {
    _ppairs = ppairs;
    _CPR = CPR;
    _offset = offset;
    MechPosition = 0;
    out_old = 0;
    oldVel = 0;
    raw = 0;
    
    __GPIOA_CLK_ENABLE();
 
    GPIOA->MODER   |= GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1 ;           //PA6 & PA7 as Alternate Function   /*!< GPIO port mode register,               Address offset: 0x00      */
    GPIOA->OTYPER  |= GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7 ;                 //PA6 & PA7 as Inputs               /*!< GPIO port output type register,        Address offset: 0x04      */
    GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7 ;     //Low speed                         /*!< GPIO port output speed register,       Address offset: 0x08      */
    GPIOA->PUPDR   |= GPIO_PUPDR_PUPDR6_1 | GPIO_PUPDR_PUPDR7_1 ;           //Pull Down                         /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C      */
    GPIOA->AFR[0]  |= 0x22000000 ;                                          //AF02 for PA6 & PA7                /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
    GPIOA->AFR[1]  |= 0x00000000 ;                                          //nibbles here refer to gpio8..15   /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
   
    __TIM3_CLK_ENABLE();
 
    TIM3->CR1   = 0x0001;                                                   // CEN(Counter ENable)='1'     < TIM control register 1
    TIM3->SMCR  = TIM_ENCODERMODE_TI12;                                     // SMS='011' (Encoder mode 3)  < TIM slave mode control register
    TIM3->CCMR1 = 0x1111;                                                   // CC1S='01' CC2S='01'         < TIM capture/compare mode register 1, maximum digital filtering
    TIM3->CCMR2 = 0x0000;                                                   //                             < TIM capture/compare mode register 2
    TIM3->CCER  = 0x0011;                                                   // CC1P CC2P                   < TIM capture/compare enable register
    TIM3->PSC   = 0x0000;                                                   // Prescaler = (0+1)           < TIM prescaler
    TIM3->ARR   = CPR;                                                      // IM auto-reload register
  
    TIM3->CNT = 0x000;   
    
    
    __TIM2_CLK_ENABLE();
    TIM3->CR2 = 0x030;                                                      //MMS = 101
    
    TIM2->PSC = 0x03;
    TIM2->SMCR = 0x24;                                                      //TS = 010 for ITR2, SMS = 100 (reset counter at edge)
    TIM2->CCMR1 = 0x3;                                                      // CC1S = 11, IC1 mapped on TRC
    
    TIM2->CCER |= TIM_CCER_CC1P;
    TIM2->CCER |= TIM_CCER_CC1E;
    
    
    TIM2->CR1 = 0x01;                                                       //CEN,  enable timer
    
    TIM3->CR1   = 0x01;                                                     // CEN
    ZPulse = new InterruptIn(PC_4);
    ZSense = new DigitalIn(PC_4);
    ZPulse->enable_irq();
    ZPulse->rise(this, &PositionSensorEncoder::ZeroEncoderCount);
    ZPulse->mode(PullDown);
    flag = 0;
    }
    
void PositionSensorEncoder::Sample(float dt){
    
    }

 
float PositionSensorEncoder::GetMechPosition() {                            //returns rotor angle in radians.
    int raw = TIM3->CNT;
    float unsigned_mech = (6.28318530718f/(float)_CPR) * (float) ((raw)%_CPR);
    return (float) unsigned_mech;// + 6.28318530718f* (float) rotations;
}

float PositionSensorEncoder::GetElecPosition() {                            //returns rotor electrical angle in radians.
    int raw = TIM3->CNT;
    float elec = ((6.28318530718f/(float)_CPR) * (float) ((_ppairs*raw)%_CPR)) - _offset;
    if(elec < 0) elec += 6.28318530718f;
    return elec;
}


    
float PositionSensorEncoder::GetMechVelocity(){

    float out = 0;
    float rawPeriod = TIM2->CCR1; //Clock Ticks
    int currentTime = TIM2->CNT;
    if(currentTime > 2000000){rawPeriod = currentTime;}
    float  dir = -2.0f*(float)(((TIM3->CR1)>>4)&1)+1.0f;    // +/- 1
    float meas = dir*180000000.0f*(6.28318530718f/(float)_CPR)/rawPeriod; 
    if(isinf(meas)){ meas = 1;}
    out = meas;   
 
    oldVel = meas;
    out_old = out;
    int n = 16;
    float sum = out;
    for (int i = 1; i < (n); i++){
        velVec[n - i] = velVec[n-i-1];
        sum += velVec[n-i];
        }
    velVec[0] = out;
    return sum/(float)n;
    }
    
float PositionSensorEncoder::GetElecVelocity(){
    return _ppairs*GetMechVelocity();
    }
    
void PositionSensorEncoder::ZeroEncoderCount(void){
    if (ZSense->read() == 1 & flag == 0){
        if (ZSense->read() == 1){
            GPIOC->ODR ^= (1 << 4);   
            TIM3->CNT = 0x000;
            GPIOC->ODR ^= (1 << 4);
        }
        }
    }

void PositionSensorEncoder::ZeroPosition(void){
    
    }
    
void PositionSensorEncoder::ZeroEncoderCountDown(void){
    if (ZSense->read() == 0){
        if (ZSense->read() == 0){
            GPIOC->ODR ^= (1 << 4);
            flag = 0;
            float dir = -2.0f*(float)(((TIM3->CR1)>>4)&1)+1.0f;
            if(dir != dir){
                dir = dir;
                rotations +=  dir;
                }

            GPIOC->ODR ^= (1 << 4);

        }
        }
    }
void PositionSensorEncoder::SetElecOffset(float offset){
    
    }
    
int PositionSensorEncoder::GetRawPosition(void){
    return 0;
    }
    
int PositionSensorEncoder::GetCPR(){
    return _CPR;
    }
    

void PositionSensorEncoder::WriteLUT(int new_lut[128]){
    memcpy(offset_lut, new_lut, 128*4);
    }
