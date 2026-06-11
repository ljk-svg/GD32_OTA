#ifndef DELAY_H
#define DELAY_H

#include "stdint.h"

void Delay_Init(void);
void Delay_Us(uint16_t us);
void Delay_Ms(uint16_t ms);

#endif
