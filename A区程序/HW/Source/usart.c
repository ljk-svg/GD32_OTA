#include "usart.h"
#include "gd32f10x.h"
#include "os_port.h"   
#include "ota_fsm.h"  

uint8_t U0_RxBuff[U0_RX_MAX]; // 永远固定在这里！
uint8_t U0_TxBuff[U0_TX_SIZE];

extern OSAL_Queue_t OTA_MsgQueue;
static OSAL_Mutex_t printf_mutex = NULL;

void DMA_Init(void) {
    dma_parameter_struct dma_init_struct;
    rcu_periph_clock_enable(RCU_DMA0);
    dma_deinit(DMA0, DMA_CH4);  
    
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(USART0); 
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_addr = (uint32_t)U0_RxBuff; // ?? 永远只指向这一个安全的打靶区！
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = U0_RX_MAX;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    
    dma_init(DMA0, DMA_CH4, &dma_init_struct);
    dma_circulation_disable(DMA0, DMA_CH4);	
    dma_channel_enable(DMA0, DMA_CH4);
}

void Usart0_Init(uint32_t bandrate) {
    if(printf_mutex == NULL) {
        printf_mutex = OSAL_MutexCreate();
    }

    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_GPIOA);
    
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
    
    usart_deinit(USART0);
    usart_baudrate_set(USART0, bandrate);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    
    usart_dma_receive_config(USART0, USART_DENR_ENABLE);
    
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); 
    nvic_irq_enable(USART0_IRQn, 5, 0); 
    usart_interrupt_enable(USART0, USART_INT_IDLE);
    
    DMA_Init();
    usart_enable(USART0);
}

// 打印函数保持你的原样，极其优秀，不改一字！
void u0_printf(char *format, ...) {
    uint16_t i;
    va_list listdata;
    
    if(printf_mutex != NULL) OSAL_MutexTake(printf_mutex, OSAL_WAIT_FOREVER);

    va_start(listdata, format);
    vsnprintf((char *)U0_TxBuff, U0_TX_SIZE, format, listdata);
    va_end(listdata);

    for(i = 0; i < strlen((const char*)U0_TxBuff); i++) {
        while(usart_flag_get(USART0, USART_FLAG_TBE) != 1);
        usart_data_transmit(USART0, U0_TxBuff[i]);
    }
    while(usart_flag_get(USART0, USART_FLAG_TC) != 1);

    if(printf_mutex != NULL) OSAL_MutexGive(printf_mutex);
}

// ==========================================
// ?? 串口 0 终极极简中断服务函数 (一击致命版)
// ==========================================
void USART0_IRQHandler(void)
{
    if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET)
    {
        usart_data_receive(USART0); // 清除 IDLE
        dma_channel_disable(DMA0, DMA_CH4);
        
        uint16_t rx_len = U0_RX_MAX - dma_transfer_number_get(DMA0, DMA_CH4);
        
        if(rx_len > 0 && rx_len <= U0_RX_MAX)
        {
            if (OTA_MsgQueue != NULL) 
            {
                OTA_Packet_t pkt;
                pkt.length = rx_len;
                // ?? 永远只从 U0_RxBuff 复制数据！绝对不会跑飞！
                memcpy(pkt.payload, U0_RxBuff, rx_len);
                OSAL_QueueSendFromISR(OTA_MsgQueue, &pkt);
            }
        }
        
        // ?? 极其暴力：永远让 DMA 指回 U0_RxBuff 的原点，重新装弹！
        dma_memory_address_config(DMA0, DMA_CH4, (uint32_t)U0_RxBuff);
        dma_transfer_number_config(DMA0, DMA_CH4, U0_RX_MAX); 
        dma_channel_enable(DMA0, DMA_CH4);
    }
}
