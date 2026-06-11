#ifndef W25Q64_H
#define W25Q64_H

#include "stdint.h"



void W25Q64_Init(void);
void W25Q64_WaitBusy(void);
void W25Q64_Enable(void);
void W25Q64_Erase64K(uint8_t blockNB);
void W25Q64_PageWrite(uint8_t *wbuff, uint16_t pageNB);
void W25Q64_Read(uint8_t *rbuff, uint32_t addr, uint32_t datalen);



#endif
