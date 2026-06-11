#ifndef USART_H
#define USART_H

#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

// ?? B区极简兵器库：只管初始化，只管往外喷日志！
void Usart0_Init(uint32_t bandrate);
void u0_printf(char *format,...);

#endif /* USART_H */