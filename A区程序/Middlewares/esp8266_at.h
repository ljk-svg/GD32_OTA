#ifndef ESP8266_AT_H
#define ESP8266_AT_H

#include <stdint.h>
#include "os_port.h"
#include "hw_usart1.h" // ?? 包含头文件就能认得 WIFI_Packet_t，不用再写一遍了！

extern OSAL_Queue_t WIFI_MsgQueue;

// 只声明！
uint8_t ESP8266_SendCmd(char *cmd, char *expect_ack, uint32_t timeout_ms);

#endif /* ESP8266_AT_H */
