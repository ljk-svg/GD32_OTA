#include "os_port.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"  // ?? 极其致命！FreeRTOS 的互斥锁必须包含这个文件！
#include "queue.h"
// ==========================================
// 延时与临界区
// ==========================================
void OSAL_DelayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms)); 
}

void OSAL_EnterCritical(void) {
    taskENTER_CRITICAL();
}

void OSAL_ExitCritical(void) {
    taskEXIT_CRITICAL();
}

// ==========================================
//  互斥锁抽象层实现 (强制类型转换的暴力美学)
// ==========================================
OSAL_Mutex_t OSAL_MutexCreate(void) {
    // 创建锁，并强转为我们自己的类型 OSAL_Mutex_t (也就是 void*)
    return (OSAL_Mutex_t)xSemaphoreCreateMutex();
}

void OSAL_MutexTake(OSAL_Mutex_t mutex, uint32_t timeout) {
    if (mutex != NULL) {
        // 如果是死等，用 FreeRTOS 的 portMAX_DELAY
        TickType_t ticks = (timeout == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
        // 把我们的 void* 句柄强转回 FreeRTOS 的 SemaphoreHandle_t
        xSemaphoreTake((SemaphoreHandle_t)mutex, ticks);
    }
}

void OSAL_MutexGive(OSAL_Mutex_t mutex) {
    if (mutex != NULL) {
        xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}
//  创建队列
OSAL_Queue_t OSAL_QueueCreate(uint32_t queue_length, uint32_t item_size) {
    return (OSAL_Queue_t)xQueueCreate(queue_length, item_size);
}

//  接收队列 (将毫秒转换为 OS 的 Tick，并支持 OSAL_WAIT_FOREVER)
uint8_t OSAL_QueueReceive(OSAL_Queue_t queue, void *item, uint32_t timeout_ms) {
    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if(xQueueReceive((QueueHandle_t)queue, item, ticks) == pdTRUE) {
        return 1; // 成功
    }
    return 0; // 超时或失败
}
// 启动任务调度器
void OSAL_vTaskStartScheduler(void){

	vTaskStartScheduler();
}
//任务创建
OSAL_BaseType_t OSAL_xTaskCreate(
    TaskFunction_t pxTaskCode,
    const char * const pcName,
    const uint16_t usStackDepth,
    void * const pvParameters,
    unsigned long uxPriority,
    OSAL_TaskHandle_t * const pxCreatedTask
)
{
    // 直接调用，不要重写参数类型！
    return xTaskCreate(
        pxTaskCode,
        pcName,
        usStackDepth,
        pvParameters,
        uxPriority,
        pxCreatedTask
    );
}


uint8_t OSAL_QueueSend(OSAL_Queue_t queue, void *item, uint32_t timeout_ms) {
    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if(xQueueSend((QueueHandle_t)queue, item, ticks) == pdPASS) {
        return 1; // 成功
    }
    return 0; // 超时或失败
}

//  架构师的神来之笔：中断发送封装！
// 直接把 FreeRTOS 极其恶心的 xHigherPriorityTaskWoken 和 portYIELD_FROM_ISR 封死在底层！
uint8_t OSAL_QueueSendFromISR(OSAL_Queue_t queue, void *item) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t res = xQueueSendFromISR((QueueHandle_t)queue, item, &xHigherPriorityTaskWoken);
    
    // 自动处理任务切换！应用层根本不需要知道什么是上下文切换！
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    
    return (res == pdTRUE) ? 1 : 0;
}




