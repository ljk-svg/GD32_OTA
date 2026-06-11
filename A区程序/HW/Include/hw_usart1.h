#ifndef HW_USART1_H
#define HW_USART1_H

#include <stdint.h>

//  全局唯一：包结构体只在这里定义一次！
typedef struct {
    uint16_t length;
    uint8_t payload[512];
} WIFI_Packet_t;

void USART1_Init(uint32_t baudrate);
void USART1_SendString(char *str);
void USART1_SendData(uint8_t *data, uint16_t len); 
#endif /* HW_USART1_H */

