#ifndef OS_PORT_H
#define OS_PORT_H

#include <stdint.h>
#include <stddef.h> 
#include "FreeRTOS.h"
#include "task.h"
// ==========================================
//  架构师的类型遮羞布：用 void* 隐藏底层真实句柄
// ==========================================
typedef void * OSAL_Mutex_t;
typedef BaseType_t OSAL_BaseType_t;
typedef TaskHandle_t OSAL_TaskHandle_t;
// 定义死等宏
#define OSAL_WAIT_FOREVER  0xFFFFFFFF

// ==========================================
// OSAL 接口声明
// ==========================================
void OSAL_DelayMs(uint32_t ms);
void OSAL_EnterCritical(void);
void OSAL_ExitCritical(void);
void OSAL_vTaskStartScheduler(void);
// 互斥锁接口
OSAL_Mutex_t OSAL_MutexCreate(void);
void OSAL_MutexTake(OSAL_Mutex_t mutex, uint32_t timeout);
void OSAL_MutexGive(OSAL_Mutex_t mutex);

typedef void* OSAL_Queue_t;

// OSAL 队列 API 声明
OSAL_Queue_t OSAL_QueueCreate(uint32_t queue_length, uint32_t item_size);
uint8_t OSAL_QueueReceive(OSAL_Queue_t queue, void *item, uint32_t timeout_ms);
uint8_t OSAL_QueueSend(OSAL_Queue_t queue, void *item, uint32_t timeout_ms);
// 中断专用的发送接口 (极其关键，内部自动处理上下文切换！)
uint8_t OSAL_QueueSendFromISR(OSAL_Queue_t queue, void *item);
OSAL_BaseType_t OSAL_xTaskCreate(
    TaskFunction_t pxTaskCode,
    const char * const pcName,
    const uint16_t usStackDepth,
    void * const pvParameters,
    unsigned long uxPriority,
    OSAL_TaskHandle_t * const pxCreatedTask
);


#endif /* OS_PORT_H */
