#include "w25q64.h"
#include "spi.h"
#include "gd32f10x.h"                   // Device header

void W25Q64_Init(void){
	rcu_periph_clock_enable(RCU_GPIOA);
	gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4);
	CS_DISENABLE;
	SPI0_Init();	
}

void W25Q64_WaitBusy(void){
	uint8_t res;
	do{
		CS_ENABLE;
		SPI0_ReadWriteByte(0x05);
		res = SPI0_ReadWriteByte(0xff);
		CS_DISENABLE;
	}while((res & 0x01) == 0x01);
}

void W25Q64_Enable(void){
	W25Q64_WaitBusy();
	CS_ENABLE;
	SPI0_ReadWriteByte(0x06);
	CS_DISENABLE;
}

//  修复：强制将 blockNB 转换为 32 位再计算！
void W25Q64_Erase64K(uint8_t blockNB){
	uint8_t wdata[4];
	uint32_t addr = (uint32_t)blockNB * 64 * 1024; // 极其严谨的绝对物理地址
	
	wdata[0] = 0xD8;
	wdata[1] = addr >> 16;
	wdata[2] = addr >> 8;
	wdata[3] = addr >> 0;
	
	W25Q64_WaitBusy();
	W25Q64_Enable();
	CS_ENABLE;
	SPI0_Write(wdata, 4);
	CS_DISENABLE;
	W25Q64_WaitBusy();
}

//  修复：强制将 pageNB 转换为 32 位再计算！
void W25Q64_PageWrite(uint8_t *wbuff, uint16_t pageNB){
	uint8_t wdata[4];
	uint32_t addr = (uint32_t)pageNB * 256; // 极其严谨的绝对物理地址
	
	wdata[0] = 0x02;
	wdata[1] = addr >> 16;
	wdata[2] = addr >> 8;
	wdata[3] = addr >> 0;
	
	W25Q64_WaitBusy();
	W25Q64_Enable();
	CS_ENABLE;
	SPI0_Write(wdata, 4);
	SPI0_Write(wbuff, 256);
	CS_DISENABLE;
}

void W25Q64_Read(uint8_t *rbuff, uint32_t addr, uint32_t datalen){
	uint8_t wdata[4];
	
	wdata[0] = 0x03;
	wdata[1] = addr >> 16;
	wdata[2] = addr >> 8;
	wdata[3] = addr >> 0;
	
	W25Q64_WaitBusy();
	CS_ENABLE;
	SPI0_Write(wdata, 4);
	SPI0_Read(rbuff, datalen);
	CS_DISENABLE;
}
