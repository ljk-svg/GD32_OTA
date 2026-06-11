#ifndef __TASK_WDT_H__
#define __TASK_WDT_H__

#include <stdint.h>

// 1. 定义各任务的签到 ID
#define WDT_TASK_ONENET  (1 << 0)  

extern volatile uint8_t g_ota_running;
// 2. 核心修正：全员存活的及格线，现在只要求 ONENET 活着就行
#define WDT_TASK_ALL     (WDT_TASK_ONENET) 

void WDT_Task(void *pvParameters);
void WDT_Set_Alive(uint32_t task_bit); 

#endif