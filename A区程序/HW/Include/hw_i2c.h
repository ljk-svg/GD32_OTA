#ifndef IIC_H
#define IIC_H

#include <stdint.h>     
#include "gd32f10x.h"  
#define IIC_SCL_H   gpio_bit_set(GPIOB,GPIO_PIN_6)
#define IIC_SCL_L   gpio_bit_reset(GPIOB,GPIO_PIN_6)

#define IIC_SDA_H   gpio_bit_set(GPIOB,GPIO_PIN_7)
#define IIC_SDA_L   gpio_bit_reset(GPIOB,GPIO_PIN_7)

#define READ_SDA    gpio_input_bit_get(GPIOB,GPIO_PIN_7)

void HW_I2C_Init(void);
void HW_I2C_Start(void);
void HW_I2C_Stop(void);
void HW_I2C_Send_Byte(uint8_t txd);
uint8_t HW_I2C_Wait_Ack(void);
uint8_t HW_I2C_Read_Byte(uint8_t ack);


#endif
