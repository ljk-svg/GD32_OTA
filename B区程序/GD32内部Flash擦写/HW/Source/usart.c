#include "gd32f10x.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

//  B区只需要极其卑微的 256 字节发送缓存就够了！
char U0_TxBuff[256];

void Usart0_Init(uint32_t bandrate){
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_GPIOA);
    
    //  只配置 TX (PA9) 即可，连 RX 都可以不管！(配了也无所谓)
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    
    usart_deinit(USART0);
    usart_baudrate_set(USART0, bandrate);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    
    //  绝情阉割：只开发送！关闭接收！关闭DMA！关闭中断！
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    // usart_receive_config(USART0, USART_RECEIVE_ENABLE); // 删！
    // usart_dma_receive_config(USART0, USART_DENR_ENABLE); // 删！
    // nvic_irq_enable(...); // 删！
    
    usart_enable(USART0);
}

//  极简轮询打印，绝对防溢出
void u0_printf(char *format, ...) {
    uint16_t i;
    va_list listdata;
    
    va_start(listdata, format);
    // 工业级防御：用 vsnprintf 代替 vsprintf，哪怕格式化错了也绝对不会把 RAM 写穿！
    vsnprintf(U0_TxBuff, sizeof(U0_TxBuff), format, listdata);
    va_end(listdata);

    for(i = 0; i < strlen(U0_TxBuff); i++){
        // 死等发送寄存器空 (TBE)
        while(usart_flag_get(USART0, USART_FLAG_TBE) != SET);
        usart_data_transmit(USART0, U0_TxBuff[i]);
    }
    
    // 极其关键：死等最后一个字节完全从移位寄存器飞出去 (TC)，保证 Jump 之前遗言绝对说完！
    while(usart_flag_get(USART0, USART_FLAG_TC) != SET);
}
