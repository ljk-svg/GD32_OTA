#include "hw_cpu.h"
#include "gd32f10x.h"

//// 定义函数指针类型，用于承载 Reset_Handler 的地址
//typedef void (*iapfun)(void);

//// ==========================================
////  IAP 终极点火跳转函数 (Bootloader 专用)
//// address: APP 区的起始地址 (例如 0x08004000)
//// ==========================================
//void HW_CPU_JumpToAddress(uint32_t address)
//{
//    iapfun jump2app;
//    
//    // 1. 安全校验：检查栈顶指针是否在 RAM 区 (GD32 RAM 起始地址是 0x20000000)
//    // 如果 Flash 里面是空的(0xFFFFFFFF)，这里直接拦截，防砖！
//    if(((*(uint32_t*)address) & 0x2FFE0000) == 0x20000000) 
//    {
//        //  2. 大厂终极清理术：把所有外设打回原形！
//        
//        // 魔法指令 1：复位整个时钟树，所有外设（串口、SPI等）瞬间断电，彻底绞杀残留中断！
//        rcu_deinit(); 
//        
//        // 魔法指令 2：暴力拔掉 SysTick（系统滴答定时器）的电源！
//        // 极其重要：防止 APP 刚启动就被 Bootloader 遗留的心跳打死！
//        SysTick->CTRL = 0;
//        SysTick->LOAD = 0;
//        SysTick->VAL = 0;

//        // 魔法指令 3：关闭全局中断 (在 APP 的 main 函数里，系统会重新打开)
//        __disable_irq(); 
//        
//        // 3. 锁定新程序的复位中断服务函数 (Reset_Handler) 的地址
//        jump2app = (iapfun)*(uint32_t*)(address + 4);
//        
//        // 4. 重置主堆栈指针 (MSP)，让 CPU 指向 APP 的 RAM 区
//        __set_MSP(*(uint32_t*)address); 
//        
//        // 5. 点火！永不回头！(一去不复返)
//        jump2app(); 
//    }

//}

//// ==========================================
////  强行软重启函数 (APP 专用)
//// 当 OTA 接收完毕，写完 AT24C02 标志位后，调用此函数强行重启进入 Bootloader！
//// ==========================================
//void HW_CPU_Reset(void)
//{
//    // 调用 ARM Cortex-M 内核的系统复位接口
//    NVIC_SystemReset();
//}


void HW_CPU_JumpToAddress(uint32_t address)
{
    typedef void (*iapfun)(void);
    iapfun jump2app;

    // 1. 栈顶合法性检查
    if (((*(uint32_t*)address) & 0x2FFE0000) == 0x20000000) 
    {
        // 2. 彻底关掉所有中断，不给 Bootloader 留任何后患
        __disable_irq(); 

        // 3. 暴力关闭所有 NVIC 中断通道
        for (uint8_t i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFF; // 关闭使能
            NVIC->ICPR[i] = 0xFFFFFFFF; // 清除悬挂
        }

        // 4. 关闭系统滴答定时器，防止它在跳转瞬间弹出一个中断
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // 5. 复位时钟树，把所有外设频率打回原形
        rcu_deinit();

        // 6. 提取 Reset_Handler 地址并重置栈指针
        jump2app = (iapfun)*(uint32_t*)(address + 4);
        __set_MSP(*(uint32_t*)address); 

        // 7. 永不回头的跳转
        jump2app(); 
    }
}
