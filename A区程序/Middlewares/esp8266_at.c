#include "esp8266_at.h"
#include "usart.h" 
#include <string.h>

uint8_t ESP8266_SendCmd(char *cmd, char *expect_ack, uint32_t timeout_ms) 
{
    static WIFI_Packet_t local_rx;
    uint32_t wait_time = 0;

    // 1. 彻底排空队列（防止残留干扰）
    while(OSAL_QueueReceive(WIFI_MsgQueue, &local_rx, 0));

    // 2. 打印并发送
    u0_printf("[Debug] 执行发送...\r\n"); 
    USART1_SendString(cmd); 
    u0_printf("[Debug] 发送完成，等待回应...\r\n"); 

    // 3. 进入带监控的等待循环
    while(wait_time < timeout_ms) 
    {
        // 阻塞式等待 10ms，既不占用 CPU 也不漏包
        if(OSAL_QueueReceive(WIFI_MsgQueue, &local_rx, 10) == 1) 
        {
            local_rx.payload[local_rx.length] = '\0';
            
            // 【关键调试点】把收到的原始数据打出来！
            u0_printf("<< [ESP-Raw]: %s\r\n", local_rx.payload); 

            if(strstr((char*)local_rx.payload, expect_ack) != NULL) {
                u0_printf("[Debug] 匹配成功: %s\r\n", expect_ack);
                return 1;
            }
            if(strstr((char*)local_rx.payload, "ERROR") != NULL) {
                 u0_printf("[Debug] ESP 返回了 ERROR\r\n");
                 return 0;
            }
        }
        wait_time += 10;
        
        // 每过 2 秒还没反应，报个警
        if(wait_time % 2000 == 0) {
            u0_printf("[Debug] 还在等 %s，已耗时 %dms...\r\n", expect_ack, wait_time);
        }
    }
    
    u0_printf("[AT-Engine] 彻底超时！指令: %s", cmd);
    return 0; 
}
