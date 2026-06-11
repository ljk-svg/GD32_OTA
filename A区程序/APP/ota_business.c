#include "ota_fsm.h"
#include "w25q64.h"
#include "at24c02.h"
#include "hw_cpu.h"
#include "usart.h"
#include <stddef.h> // NULL

// 张三写的：处理启动事件
static OTA_State_t Action_HandleStart(uint8_t *packet, uint16_t len) {
    ota_ctx.firmware_size = (packet[3]<<24) | (packet[4]<<16) | (packet[5]<<8) | packet[6];
    ota_ctx.total_crc     = (packet[7]<<8)  | packet[8];
    ota_ctx.received_size = 0;
    ota_ctx.flash_offset  = 0;
    
    u0_printf("[Business] 启动OTA！擦除Flash...\r\n");
    W25Q64_Erase64K(0); 
    return OTA_STATE_RECEIVING; 
}

// 李四写的：处理接收事件
static OTA_State_t Action_HandleData(uint8_t *packet, uint16_t len) {
    uint16_t data_len = (packet[3]<<8) | packet[4];
    uint8_t *payload = &packet[5];
    
    W25Q64_PageWrite(payload, ota_ctx.flash_offset / 256);
    ota_ctx.flash_offset += data_len;
    ota_ctx.received_size += data_len;
    
    if (ota_ctx.received_size >= ota_ctx.firmware_size) {
        u0_printf("[Business] 接收完毕，校验并重启...\r\n");
        // 假设校验通过
        uint8_t flag = 0x5A;
        AT24C02_WriteBuffer(0x00, &flag, 1);
        HW_CPU_Reset(); 
    }
    return OTA_STATE_RECEIVING;
}

// ==========================================
//  集中装载区 (大家把自己的函数都在这里注册一下)
// ==========================================
void OTA_Logic_Install(void) {
    // 动态注册！极度优雅！
    OTA_FSM_Register(OTA_STATE_IDLE,      OTA_EVENT_START, Action_HandleStart);
    OTA_FSM_Register(OTA_STATE_RECEIVING, OTA_EVENT_DATA,  Action_HandleData);
    
    // 如果以后王五加了“中断”功能，他只要在这里加一行：
    // OTA_FSM_Register(OTA_STATE_RECEIVING, OTA_EVENT_ABORT, Action_HandleAbort);
}
