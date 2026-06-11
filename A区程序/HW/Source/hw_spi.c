#include "hw_spi.h"
#include "gd32f10x.h"
#include "os_port.h"   //  引入极其重要的操作系统抽象层！



// 定义一把 SPI0 的专属锁！
static OSAL_Mutex_t spi0_mutex = NULL; 

void HW_SPI0_Init(void)
{
    spi_parameter_struct spi_parameter;
    
    //  1. 创建这把总线锁！(底层调用 FreeRTOS 的 xSemaphoreCreateMutex)
    if (spi0_mutex == NULL) {
        spi0_mutex = OSAL_MutexCreate(); 
    }

    rcu_periph_clock_enable(RCU_SPI0);
    rcu_periph_clock_enable(RCU_GPIOA);
  
	gpio_init(GPIOA,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,GPIO_PIN_4); //初始话CS
	
	
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5|GPIO_PIN_7);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    
    spi_i2s_deinit(SPI0);
    spi_parameter.device_mode = SPI_MASTER;
    spi_parameter.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_parameter.frame_size = SPI_FRAMESIZE_8BIT;
    spi_parameter.nss = SPI_NSS_SOFT; // 极其正确！CS（片选）引脚必须用软件控制！
    spi_parameter.endian = SPI_ENDIAN_MSB;
    spi_parameter.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi_parameter.prescale = SPI_PSC_2;
    spi_init(SPI0, &spi_parameter);
    spi_enable(SPI0);	
}
void HW_SPI0_CS_LOW(void)
{
	gpio_bit_reset(GPIOA,GPIO_PIN_4);
}
void HW_SPI0_CS_HIGH(void)
{
	gpio_bit_set(GPIOA,GPIO_PIN_4);
}

//  极其精妙的总线抢占接口！
// 谁想用 SPI0，必须先调用 Lock 拿到钥匙！拿不到就在这死等（挂起任务，不占CPU）！
void HW_SPI0_Lock(void) {
    if(spi0_mutex != NULL) {
        OSAL_MutexTake(spi0_mutex, OSAL_WAIT_FOREVER);
    }
}

//  用完了必须还钥匙！
void HW_SPI0_Unlock(void) {
    if(spi0_mutex != NULL) {
        OSAL_MutexGive(spi0_mutex);
    }
}

// 底层纯粹的收发（不上锁，锁留给上层调用！）
uint8_t HW_SPI0_ReadWriteByte(uint8_t txd){
    while(spi_i2s_flag_get(SPI0, SPI_FLAG_TBE) != 1);
    spi_i2s_data_transmit(SPI0, txd);
    while(spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE) != 1);
    return spi_i2s_data_receive(SPI0);
}
