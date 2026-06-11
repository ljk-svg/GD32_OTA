#ifndef AT24C02_H
#define AT24C02_H

#include <stdint.h>

#define AT24C02_WADDR  0xA0
#define AT24C02_RADDR  0xA1

uint8_t AT24C02_Init(void);
uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata);
uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen);

// 高级功能：写连续数据（内部自动处理翻页和延时）
uint8_t AT24C02_WriteBuffer(uint8_t addr, uint8_t *wdata, uint16_t datalen);

#endif
