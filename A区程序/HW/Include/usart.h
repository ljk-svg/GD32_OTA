#ifndef USART_H
#define USART_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define U0_RX_MAX  256
#define U0_TX_SIZE 512

// 只需要一个固定的打靶区！
extern uint8_t U0_RxBuff[U0_RX_MAX];

void Usart0_Init(uint32_t bandrate);
void u0_printf(char *format, ...);

#endif /* USART_H */

