#include "spi.h"
#include "gd32f10x.h"                   // Device header

void SPI0_Init(void){
	spi_parameter_struct spi_parameter;
	rcu_periph_clock_enable(RCU_SPI0);
	rcu_periph_clock_enable(RCU_GPIOA);
	
	gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5|GPIO_PIN_7);
	gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
	
	spi_i2s_deinit(SPI0);
	spi_parameter.device_mode = SPI_MASTER;
	spi_parameter.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
	spi_parameter.frame_size = SPI_FRAMESIZE_8BIT;
	spi_parameter.nss = SPI_NSS_SOFT;
	spi_parameter.endian = SPI_ENDIAN_MSB;
	spi_parameter.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
	
	// ?? Bootloader 第一法则：求稳！改为 8 分频，彻底隔绝干扰！
	spi_parameter.prescale = SPI_PSC_8; 
	
	spi_init(SPI0, &spi_parameter);
	spi_enable(SPI0);	
}

uint8_t SPI0_ReadWriteByte(uint8_t txd){
	while(spi_i2s_flag_get(SPI0, SPI_FLAG_TBE) != 1);
	spi_i2s_data_transmit(SPI0, txd);
	while(spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE) != 1);
	return spi_i2s_data_receive(SPI0);
}

void SPI0_Write(uint8_t *wdata, uint16_t datalen){
	uint16_t i;
	for(i=0; i<datalen; i++){
		SPI0_ReadWriteByte(wdata[i]);
	}
}

void SPI0_Read(uint8_t *rdata, uint16_t datalen){
	uint16_t i;
	for(i=0; i<datalen; i++){
		rdata[i] = SPI0_ReadWriteByte(0xff);
	}
}
