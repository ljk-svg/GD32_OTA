#include "w25q64.h"
#include "hw_spi.h"   //  调用第二层 SPI 接口 (包含锁和收发)
#include "os_port.h"  //  调用操作系统的延时



void W25Q64_Init(void) {
    HW_SPI0_Init(); // 底层硬件初始化 (包含 GPIO 和 SPI 外设配置)
    HW_SPI0_CS_HIGH();
}

// ==========================================
//  架构师的屠龙技：非阻塞状态轮询！
// ==========================================
void W25Q64_WaitBusy(void) {
    uint8_t res;
    while(1) {
        // 1. 拿钥匙，占用总线查状态 (只占几十微秒)
        HW_SPI0_Lock();
        HW_SPI0_CS_LOW();
        HW_SPI0_ReadWriteByte(0x05);
        res = HW_SPI0_ReadWriteByte(0xFF);
        HW_SPI0_CS_HIGH();
        HW_SPI0_Unlock(); // 2. 查完立刻归还钥匙！

        if ((res & 0x01) == 0) {
            break; // 不忙了，跳出循环！
        }
        
        //  3. 终极灵魂：Flash 还在擦除，直接挂起当前任务 2 毫秒！
        // 在这 2 毫秒里，不仅 CPU 让给了其他任务，连 SPI0 总线也让出来了！
        // 如果这时候屏幕任务想刷新画面，完全可以使用 SPI0 总线！绝不卡死！
        OSAL_DelayMs(2); 
    }
}

void W25Q64_Enable(void) {
    // 写使能是瞬间的，直接一把梭
    HW_SPI0_Lock();
    HW_SPI0_CS_LOW();
    HW_SPI0_ReadWriteByte(0x06);
    HW_SPI0_CS_HIGH();
    HW_SPI0_Unlock();
}

void W25Q64_Erase64K(uint8_t blockNB) {
    W25Q64_WaitBusy(); // 等待之前的工作完成
    W25Q64_Enable();   // 写使能
    
    //  发送擦除指令 (必须加锁保护完整波形)
    HW_SPI0_Lock();
    HW_SPI0_CS_LOW();
    HW_SPI0_ReadWriteByte(0xD8);
    HW_SPI0_ReadWriteByte((blockNB * 64 * 1024) >> 16);
    HW_SPI0_ReadWriteByte((blockNB * 64 * 1024) >> 8);
    HW_SPI0_ReadWriteByte((blockNB * 64 * 1024) >> 0);
    HW_SPI0_CS_HIGH();
    HW_SPI0_Unlock();
    
    W25Q64_WaitBusy(); //  极其优雅地等待擦除完成 (内部会循环挂起，绝不阻塞CPU)
}

void W25Q64_PageWrite(uint8_t *wbuff, uint16_t pageNB) {
    W25Q64_WaitBusy();
    W25Q64_Enable();
    
    HW_SPI0_Lock(); // 锁定总线，发连续数据天王老子来了也不能打断
    HW_SPI0_CS_LOW();
    HW_SPI0_ReadWriteByte(0x02);
    HW_SPI0_ReadWriteByte((pageNB * 256) >> 16);
    HW_SPI0_ReadWriteByte((pageNB * 256) >> 8);
    HW_SPI0_ReadWriteByte((pageNB * 256) >> 0);
    
    for(uint16_t i = 0; i < 256; i++) {
        HW_SPI0_ReadWriteByte(wbuff[i]);
    }
    HW_SPI0_CS_HIGH();
    HW_SPI0_Unlock(); // 写完归还总线
}

void W25Q64_Read(uint8_t *rbuff, uint32_t addr, uint32_t datalen) {
    W25Q64_WaitBusy();
    
    HW_SPI0_Lock(); // 锁定总线读数据
    HW_SPI0_CS_LOW();
    HW_SPI0_ReadWriteByte(0x03);
    HW_SPI0_ReadWriteByte((addr) >> 16);
    HW_SPI0_ReadWriteByte((addr) >> 8);
    HW_SPI0_ReadWriteByte((addr) >> 0);
    
    for(uint32_t i = 0; i < datalen; i++) {
        rbuff[i] = HW_SPI0_ReadWriteByte(0xFF);
    }
    HW_SPI0_CS_HIGH();
    HW_SPI0_Unlock();
}
