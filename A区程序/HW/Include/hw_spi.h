#ifndef _HW_SPI_H_
#define _HW_SPI_H_
#include "stdint.h"

void HW_SPI0_Init(void);
void HW_SPI0_CS_LOW(void);
void HW_SPI0_CS_HIGH(void);
void HW_SPI0_Lock(void);
void HW_SPI0_Unlock(void) ;
uint8_t HW_SPI0_ReadWriteByte(uint8_t txd);

#endif
