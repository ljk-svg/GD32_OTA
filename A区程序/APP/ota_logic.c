#include "ota_fsm.h"
#include "w25q64.h"
#include "at24c02.h"
#include "hw_cpu.h"
#include "usart.h"
#include "os_port.h"

// 引入全局状态机上下文
extern OTA_Context_t ota_ctx;

// ==========================================
// 动作1：处理启动事件 (收到升级通知)
// 数据包格式假设: AA 55 01 [Size(4字节)] [CRC(2字节)]
// ==========================================
static OTA_State_t Action_HandleStart(uint8_t *packet, uint16_t len) 
{
    // 1. 解析固件总大小
    ota_ctx.firmware_size = (packet[3]<<24) | (packet[4]<<16) | (packet[5]<<8) | packet[6];
    ota_ctx.total_crc     = (packet[7]<<8)  | packet[8];
    ota_ctx.received_size = 0;
    ota_ctx.flash_offset  = 0;
    
    // --- 必须新增：初始化蓄水池指针 ---
    ota_ctx.buffer_idx    = 0; 
    
    u0_printf("[OTA-Logic] 收到升级指令！大小: %d\r\n", ota_ctx.firmware_size);
    W25Q64_Erase64K(0); 
    return OTA_STATE_RECEIVING; 
}

// ==========================================
// 动作2：处理数据包 (收到固件切片)
// 数据包格式假设: AA 55 02 [Len(2字节大端)] [Data(N字节)]
// ==========================================
static OTA_State_t Action_HandleData(uint8_t *packet, uint16_t len) 
{
    // 1. 解析数据长度（2字节大端序）和数据起点
    // 数据包索引：0=AA, 1=55, 2=02, 3=Len高字节, 4=Len低字节, 5=Data起点
    uint16_t data_len = (packet[3]<<8) | packet[4];
    uint8_t *payload = &packet[5]; // 数据从第6字节开始（索引5）
    
    // --- 【灵魂修改：字节级搬运蓄水池】 ---
    for(uint16_t i = 0; i < data_len; i++) 
    {
        // 2. 往蓄水池里填一个字节
        ota_ctx.page_buffer[ota_ctx.buffer_idx++] = payload[i];
        
        // 3. 【关键】如果凑够了256字节，瞬间烧录到W25Q64！
        if(ota_ctx.buffer_idx == 256) 
        {
            // W25Q64页写：输入是“页号”，页号=Flash地址/256（因为一页256字节）
            W25Q64_PageWrite(ota_ctx.page_buffer, ota_ctx.flash_offset / 256);
            
            ota_ctx.flash_offset += 256; // Flash偏移跳256字节（下一页的地址）
            ota_ctx.buffer_idx = 0;      // 蓄水池清空，准备填下一页
            // u0_printf("."); // 打印个点，代表写了一页（调试用，避免刷屏）
        }
    }
    
    // 4. 更新总接收进度（用于判断是否收完）
    ota_ctx.received_size += data_len;
    u0_printf("[OTA-Logic] 进度: %d / %d\r\n", ota_ctx.received_size, ota_ctx.firmware_size);
    
    // 5. 【终极判决】收完了吗？
    if (ota_ctx.received_size >= ota_ctx.firmware_size) 
    {
        // --- 【特别补丁】处理最后不足一页的残余！ ---
        if(ota_ctx.buffer_idx > 0) 
        {
            // 最后一丁点也得烧进去，W25Q64_PageWrite内部会处理“不足256字节”的情况
            W25Q64_PageWrite(ota_ctx.page_buffer, ota_ctx.flash_offset / 256);
            u0_printf("[OTA-Logic] 尾部 %d 字节补齐写入。\r\n", ota_ctx.buffer_idx);
        }

        u0_printf("\r\n[OTA-Logic] 固件对齐写入完毕！开始立遗嘱...\r\n");
        
        // 6. 【立遗嘱1】把固件大小写到AT24C02的地址0x01（4字节小端序，Bootloader读）
        uint8_t len_buf[4];
        len_buf[0] = (ota_ctx.firmware_size >> 0)  & 0xFF; // 低字节在前
        len_buf[1] = (ota_ctx.firmware_size >> 8)  & 0xFF;
        len_buf[2] = (ota_ctx.firmware_size >> 16) & 0xFF;
        len_buf[3] = (ota_ctx.firmware_size >> 24) & 0xFF;
        AT24C02_WriteBuffer(0x01, len_buf, 4);

        // 7. 【立遗嘱2】把升级标志0x5A写到AT24C02的地址0x00！
        // 【核心逻辑】Bootloader启动时，先读AT24C02地址0x00：
        // - 如果是0x5A：说明有新固件，把W25Q64的固件搬到CPU内部Flash，然后擦除0x5A，重启；
        // - 如果不是0x5A：说明没有新固件，直接启动原来的固件；
        uint8_t flag = 0x5A;
        AT24C02_WriteBuffer(0x00, &flag, 1);
        
        // 8. 【自刎】延时100ms，确保AT24C02写入完成（EEPROM写入慢，需要时间），然后CPU重启！
        OSAL_DelayMs(100); 
        HW_CPU_Reset(); 
    }
    
    // 9. 没收完，继续留在“接收状态”
    return OTA_STATE_RECEIVING;
}

// ==========================================
// 集中装载区
// ==========================================
void OTA_Logic_Install(void) {
    OTA_FSM_Register(OTA_STATE_IDLE,      0x01, Action_HandleStart); // 事件 0x01: 开始
    OTA_FSM_Register(OTA_STATE_RECEIVING, 0x02, Action_HandleData);  // 事件 0x02: 数据
}
