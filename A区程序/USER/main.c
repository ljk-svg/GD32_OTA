#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "onenet_app.h"
// 引入我们的架构组件
#include "os_port.h"
#include "usart.h"
#include "hw_usart1.h"
// 把不需要的硬件初始化也先注释掉，排除一切干扰
 #include "at24c02.h"
// #include "w25q64.h"
#include "task_wdt.h"
OSAL_Queue_t OTA_MsgQueue = NULL; 
extern void OneNET_Task(void *pvParameters); // 声明外部任务
OSAL_TaskHandle_t OneNETTask_Handler = NULL;

int main(void)
{

    // 改写向量表 (OTA必备)
   nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x4000);

    // 1. 只初始化绝对必须的硬件
    Usart0_Init(115200);
    USART1_Init(115200); 
	AT24C02_Init();
    u0_printf("\r\n=================================\r\n");
    u0_printf(" V3.0！\r\n");
	
    u0_printf("=================================\r\n");

    // 2. 把全部的内存希望都给 OneNET_Task！
    OSAL_BaseType_t ret = OSAL_xTaskCreate(OneNET_Task, "OneNET_Task", 640, NULL, 3, &OneNETTask_Handler);
    OSAL_xTaskCreate(WDT_Task, "WDT_Task", 128, NULL, 5, NULL);
	
    u0_printf("[System] OneNETTask create ret = %d\r\n", (int)ret);

    if(ret != pdPASS)
    {
        u0_printf("[Error] 卧槽！连一个任务都建不出来！必须去 FreeRTOSConfig.h 改 configTOTAL_HEAP_SIZE 了！\r\n");
        while(1);
    }

    // 3. 启动调度器
    u0_printf("[System] 正在启动 RTOS 调度器...\r\n");
    OSAL_vTaskStartScheduler();

    // 如果走到这里，说明调度器没起来
    u0_printf("[Fatal] 调度器启动失败！\r\n");

    while(1) {}
}
