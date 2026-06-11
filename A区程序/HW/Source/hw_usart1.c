#include "hw_usart1.h"
#include "gd32f10x.h"
#include "os_port.h"
#include <string.h>
#include "usart.h"

// 全局变量定义
OSAL_Queue_t WIFI_MsgQueue = NULL;
static WIFI_Packet_t s_rx_buf; 
static uint16_t s_rx_idx = 0; // 加回来！最简单粗暴的计数器

void USART1_Init(uint32_t baudrate) 
{
    if(WIFI_MsgQueue == NULL) WIFI_MsgQueue = OSAL_QueueCreate(5, sizeof(WIFI_Packet_t));

    rcu_periph_clock_enable(RCU_USART1);
    rcu_periph_clock_enable(RCU_GPIOA);
    
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_3); // 上拉输入，防干扰
    
    usart_deinit(USART1);
    usart_baudrate_set(USART1, baudrate);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    
    //  砍掉所有 DMA 配置，就留这两句开启单字节接收中断！
    nvic_irq_enable(USART1_IRQn, 12, 0); 
    usart_interrupt_enable(USART1, USART_INT_RBNE); // 只开 RBNE (接收缓冲区非空中断)
	
	usart_interrupt_enable(USART1, USART_INT_IDLE);
    usart_enable(USART1);
}

void USART1_SendString(char *str) {
    while(*str) {
        while(usart_flag_get(USART1, USART_FLAG_TBE) == RESET);
        usart_data_transmit(USART1, (uint8_t)*str++);
    }
}

void USART1_IRQHandler(void)
{
    // 1. 防溢出装甲 (保持你的原样)
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_ERR_ORERR) != RESET) {
        usart_data_receive(USART1);
    }

    // 2. 接收单个字节
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t data = usart_data_receive(USART1);
        
        if(s_rx_idx < sizeof(s_rx_buf.payload) - 1) {
            s_rx_buf.payload[s_rx_idx++] = data;
        }

        // 依然保留这个逻辑：防止单次数据量太大撑爆缓冲区
        // 如果到了 250，就先发走一波 (剩下的让 IDLE 去处理)
        if(data == '\n' || data == '>' || data == '}' || s_rx_idx >= 250) 
        {
            s_rx_buf.payload[s_rx_idx] = '\0';
            s_rx_buf.length = s_rx_idx;
            
            if(WIFI_MsgQueue != NULL) {
                OSAL_QueueSendFromISR(WIFI_MsgQueue, &s_rx_buf);
            }
            s_rx_idx = 0; 
        }
    }

    // 3. 【核心新增：无情榨干残余数据】
    // 只要总线一空闲（说明服务器发完一波了），立刻把兜里剩下的字节交出来！
    if(usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE) != RESET)
    {
        usart_data_receive(USART1); // 必须读取一次数据寄存器，用于清除 IDLE 标志位
        
        if(s_rx_idx > 0) 
        {
            s_rx_buf.payload[s_rx_idx] = '\0';
            s_rx_buf.length = s_rx_idx;
            
            if(WIFI_MsgQueue != NULL) {
                OSAL_QueueSendFromISR(WIFI_MsgQueue, &s_rx_buf);
            }
            s_rx_idx = 0; // 残包发送完毕，清零计数器
        }
    }
}
// ?? 专发二进制数据，遇到 0x00 绝对不会停！
void USART1_SendData(uint8_t *data, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) {
        while(usart_flag_get(USART1, USART_FLAG_TBE) == RESET);
        usart_data_transmit(USART1, data[i]);
    }
}
