#include "gd32f10x.h"
#include "iic.h"
#include "delay.h"

void IIC_Init(void){
	rcu_periph_clock_enable(RCU_GPIOB);	
	gpio_init(GPIOB,GPIO_MODE_OUT_OD,GPIO_OSPEED_50MHZ,GPIO_PIN_6);
	gpio_init(GPIOB,GPIO_MODE_OUT_OD,GPIO_OSPEED_50MHZ,GPIO_PIN_7);
	IIC_SCL_H;
	IIC_SDA_H;
}

void IIC_Start(void){
	IIC_SCL_H;
	IIC_SDA_H;
	Delay_Us(2);
	IIC_SDA_L;
	Delay_Us(2);
	IIC_SCL_L;
}

void IIC_Stop(void){
	IIC_SCL_H;
	IIC_SDA_L;
	Delay_Us(2);
	IIC_SDA_H;
}


void IIC_Send_Byte(uint8_t txd){
	int8_t i;
	
	for(i=7;i>=0;i--){
		IIC_SCL_L;
		if(txd&BIT(i))
			IIC_SDA_H;
		else
			IIC_SDA_L;
		Delay_Us(2);
		IIC_SCL_H;
		Delay_Us(2);
	}
	IIC_SCL_L;
	IIC_SDA_H;
}

uint8_t IIC_Wait_Ack(int16_t timeout){
	do{
		timeout--;
		Delay_Us(2);
	}while((READ_SDA)&&(timeout>=0));
	if(timeout<0) return 1;
	IIC_SCL_H;
	Delay_Us(2);
	if(READ_SDA!=0) return 2;
	IIC_SCL_L;
	Delay_Us(2);
	return 0;
}

uint8_t IIC_Read_Byte(uint8_t ack){
	int8_t i;
	uint8_t rxd;
	
	rxd = 0;
	for(i=7;i>=0;i--){
		IIC_SCL_L;
		Delay_Us(2);
		IIC_SCL_H;
		if(READ_SDA)
			rxd |= BIT(i);
		Delay_Us(2);
	}
	IIC_SCL_L;
	Delay_Us(2);
	if(ack){
		IIC_SDA_L;
		IIC_SCL_H;
		Delay_Us(2);
		IIC_SCL_L;
		IIC_SDA_H;
		Delay_Us(2);
	}else{
		IIC_SDA_H;
		IIC_SCL_H;
		Delay_Us(2);
		IIC_SCL_L;
		Delay_Us(2);
	}
	return rxd;
}


