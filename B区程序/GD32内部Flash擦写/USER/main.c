#include "gd32f10x.h"
#include "usart.h"      
#include "iic.h"
#include "m24c02.h"    
#include "w25q64.h"     
#include "fmc.h"        
#include "hw_cpu.h"     
#include "delay.h"
#include <string.h>

// 根据你的实际 APP 偏移修改
#define APP_START_ADDR       0x08004000  
#define APP_MAX_SIZE         0xC000      // 48KB
#define W25Q64_FW_ADDR       0x000000    

int main(void)
{
    uint8_t ota_flag = 0;
    uint32_t fw_size = 0;
    uint8_t len_buf[4];
    uint8_t read_buf[256];

    // 1. 硬件基础初始化
    Delay_Init();        
    Usart0_Init(115200); 
    IIC_Init();          
    W25Q64_Init();

    u0_printf("\r\n=================================\r\n");
    u0_printf("   Bootloader V2.2 工业级稳定版\r\n");
    u0_printf("=================================\r\n");

    // 2. 检查 EEPROM 标志位 (0x00 地址)
    M24C02_ReadData(0x00, &ota_flag, 1);

    if (ota_flag == 0x5A) 
    {
        // --- 读取固件长度 (0x01 地址开始的 4 个字节) ---
        M24C02_ReadData(0x01, len_buf, 4);
        fw_size = len_buf[0] | (len_buf[1] << 8) | (len_buf[2] << 16) | (len_buf[3] << 24);

        u0_printf("[Boot] 准备 OTA 升级，固件大小: %d 字节\r\n", fw_size);

        // 长度安全过滤
        if(fw_size == 0 || fw_size > APP_MAX_SIZE || fw_size == 0xFFFFFFFF) {
            u0_printf("[Error] 固件长度非法 (0 或超过 48KB)，取消升级！\r\n");
            goto JUMP_STAGE; 
        }

        // --- 擦除内部 Flash ---
        u0_printf("[Boot] 正在解除锁定并擦除 Flash...");
        GD32_EraseFlash(APP_START_ADDR, fw_size);
        u0_printf(" 擦除成功！\r\n");

        // --- 分块搬运数据 (256字节/包) ---
        u0_printf("[Boot] 开始搬运: ");
        uint32_t write_addr = APP_START_ADDR;
        uint32_t ext_addr = W25Q64_FW_ADDR;

        for(uint32_t i = 0; i < fw_size; i += 256) 
        {
            uint32_t chunk_size = (fw_size - i) >= 256 ? 256 : (fw_size - i);
            
            memset(read_buf, 0xFF, 256); // 填充缓冲区防止脏数据
            W25Q64_Read(read_buf, ext_addr, chunk_size);
            
            // 调用你重构后的 GD32_WriteFlash
            GD32_WriteFlash(write_addr, (uint32_t*)read_buf, chunk_size);
            
            ext_addr += chunk_size;
            write_addr += chunk_size;
            
            if((i % 1024) == 0) u0_printf("."); // 进度反馈
        }

        // --- 极其重要：写完后立即清除 EEPROM 标志位 ---
        ota_flag = 0x00;
        M24C02_WriteByte(0x00, ota_flag);
        u0_printf("\r\n[Boot] 固件写入完毕，标志位已归零。\r\n");
    } 
    else 
    {
        u0_printf("[Boot] 标志位 0x%02X，无需更新。\r\n", ota_flag);
    }

JUMP_STAGE:
    // 3. 最终跳转前审计 (基于你提供的 Verify 逻辑)
    u0_printf("\r\n[Verify] 正在进行最后的数据审计...\r\n");
    uint32_t *flash_ptr = (uint32_t *)APP_START_ADDR;

    u0_printf("  - MSP (堆栈指针): 0x%08X\r\n", flash_ptr[0]);
    u0_printf("  - Reset (复位向量): 0x%08X\r\n", flash_ptr[1]);

    // 检查 MSP 是否指向 RAM 范围 (0x2000XXXX)
    if((flash_ptr[0] & 0x2FFE0000) != 0x20000000) 
    {
        u0_printf("[Error] Flash 头部数据非法！跳转必死！\r\n");
        while(1) {
            u0_printf("请检查 APP 工程的 RO_BASE 设置是否为 0x08004000\r\n");
            Delay_Ms(2000);
        }
    }

    u0_printf("[Boot] 引擎点火！Jump to APP...\r\n");
    Delay_Ms(100); // 确保串口最后一条消息能发出去

    // 4. 执行你重构后的终极跳转逻辑
    HW_CPU_JumpToAddress(APP_START_ADDR);

    // 如果运行到这里，说明跳转代码出错了
    while(1) {
        u0_printf("[Critical] 跳转指令执行失败！\r\n");
        Delay_Ms(1000); 
    }
}