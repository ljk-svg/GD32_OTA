#ifndef SYS_PARAM_H
#define SYS_PARAM_H

#include <stdint.h>

// 1. 定义极其清晰的 OTA 状态机枚举
typedef enum {
    OTA_STATE_IDLE = 0,       // 空闲待机
    OTA_STATE_CONNECTING,     // 连接服务器/上位机
    OTA_STATE_DOWNLOADING,    // 正在下载固件
    OTA_STATE_VERIFYING,      // 校验固件 (CRC/MD5)
    OTA_STATE_READY_TO_BOOT,  // 准备重启进入新固件
    OTA_STATE_ERROR           // 发生极其致命的错误
} ota_state_t;

// 2. 核心 API 声明（别的 .c 文件只能通过这些函数碰数据！）
void SysParam_Init(void);

void SysParam_Set_OtaState(ota_state_t new_state);
ota_state_t SysParam_Get_OtaState(void);

void SysParam_Set_DownloadProgress(uint8_t percent);
uint8_t SysParam_Get_DownloadProgress(void);

#endif /* SYS_PARAM_H */
