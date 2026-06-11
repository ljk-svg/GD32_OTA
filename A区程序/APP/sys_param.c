#include "sys_param.h"
#include "FreeRTOS.h"
#include "semphr.h"

// ==========================================
//  架构师绝学：static 私有大结构体
// 绝对不允许任何外部文件 extern 访问它！
// ==========================================
static struct {
    ota_state_t current_ota_state;
    uint8_t     download_progress;
    // 以后你的设备ID、WiFi密码、业务参数，全部往这里面塞！
} g_sys_params;

// 定义一把保护伞（互斥锁）
static SemaphoreHandle_t param_mutex = NULL;

// 初始化函数（在 main 函数里，开启 RTOS 调度器之前调用）
void SysParam_Init(void)
{
    // 创建极其霸道的互斥锁
    param_mutex = xSemaphoreCreateMutex();
    
    // 初始化默认参数
    g_sys_params.current_ota_state = OTA_STATE_IDLE;
    g_sys_params.download_progress = 0;
}

// ==========================================
//  线程安全的 Setter (写操作)
// ==========================================
void SysParam_Set_OtaState(ota_state_t new_state)
{
    if (param_mutex != NULL) {
        // 拿锁！拿不到就死等 (portMAX_DELAY)
        xSemaphoreTake(param_mutex, portMAX_DELAY); 
        
        g_sys_params.current_ota_state = new_state;
        
        // 释放锁！让别的线程可以访问
        xSemaphoreGive(param_mutex); 
    }
}

// ==========================================
//  线程安全的 Getter (读操作)
// ==========================================
ota_state_t SysParam_Get_OtaState(void)
{
    ota_state_t state = OTA_STATE_IDLE;
    
    if (param_mutex != NULL) {
        xSemaphoreTake(param_mutex, portMAX_DELAY);
        state = g_sys_params.current_ota_state;
        xSemaphoreGive(param_mutex);
    }
    return state;
}

// 进度条的 Setter
void SysParam_Set_DownloadProgress(uint8_t percent)
{
    if (param_mutex != NULL) {
        xSemaphoreTake(param_mutex, portMAX_DELAY);
        g_sys_params.download_progress = percent;
        xSemaphoreGive(param_mutex);
    }
}

// 进度条的 Getter
uint8_t SysParam_Get_DownloadProgress(void)
{
    uint8_t percent = 0;
    if (param_mutex != NULL) {
        xSemaphoreTake(param_mutex, portMAX_DELAY);
        percent = g_sys_params.download_progress;
        xSemaphoreGive(param_mutex);
    }
    return percent;
}
