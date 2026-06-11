#ifndef SPI_H
#define SPI_H

#include "stdint.h"

void SPI0_Init(void);
uint8_t SPI0_ReadWriteByte(uint8_t txd);
void SPI0_Write(uint8_t *wdata, uint16_t datalen);
void SPI0_Read(uint8_t *rdata, uint16_t datalen);

#endif


