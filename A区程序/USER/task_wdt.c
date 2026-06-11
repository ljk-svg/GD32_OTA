#include "task_wdt.h"
#include "hw_wdt.h"      
#include "os_port.h"     
#include <stdio.h>       
#include "usart.h"

static volatile uint32_t g_task_alive_flags = 0;

// 【新增】：定义这个标志位，默认 0 (正常运行)
volatile uint8_t g_ota_running = 0; 

void WDT_Set_Alive(uint32_t task_bit)
{
    OSAL_EnterCritical(); 
    g_task_alive_flags |= task_bit;
    OSAL_ExitCritical();
}

void WDT_Task(void *pvParameters)
{
    HW_IWDG_Init(); 

    u0_printf("\r\n[WDT-Task] 看门狗已启动！给予系统 30 秒开机无敌时间...\r\n");
    for(int i = 0; i < 30; i++)
    {
        HW_IWDG_Feed(); 
        OSAL_DelayMs(1000);
    }
    
    u0_printf("\r\n[WDT-Task] 宽限期结束！开始进行多任务无情点名！\r\n");

    while(1)
    {
        OSAL_DelayMs(3000); // 3秒点一次名

        // =========================================================
        // 【核心大招】：检查是不是在 OTA 状态
        // =========================================================
        if (g_ota_running == 1) 
        {
            // 如果在 OTA，无条件喂狗！不查签到了！
            HW_IWDG_Feed(); 
            // u0_printf("[WDT-Task] OTA 免死金牌生效中，自动喂狗...\r\n"); // 嫌吵可以注释掉
        }
        else if ((g_task_alive_flags & WDT_TASK_ALL) == WDT_TASK_ALL)
        {
            // 正常模式：全员签到，正常喂狗
            HW_IWDG_Feed(); 
            g_task_alive_flags = 0; 
        }
        else
        {
            // 完蛋，有人卡死了！拒绝喂狗，等死吧！
            u0_printf("[WDT-Task] 致命错误！有人未签到！当前标志: 0x%02X\r\n", g_task_alive_flags);
        }
    }
}