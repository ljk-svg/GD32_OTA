#ifndef OTA_FSM_H
#define OTA_FSM_H

#include <stdint.h>

// ==========================================
// 1. 状态定义（枚举，清晰不混乱）
// ==========================================
typedef enum {
    OTA_STATE_IDLE = 0,      // 空闲状态：没在升级，等着收启动指令
    OTA_STATE_RECEIVING,      // 接收状态：正在收固件切片，烧录Flash
    OTA_STATE_VERIFY          // 校验状态（预留，代码里没用到，以后可以加CRC校验）
} OTA_State_t;

// ==========================================
// 2. 事件定义（指令字，对应数据包的第3字节）
// ==========================================
#define OTA_EVENT_START      0x01  // 启动事件：AA 55 01 ...
#define OTA_EVENT_DATA       0x02  // 数据事件：AA 55 02 ...

// ==========================================
// 3. 动作函数指针（核心！业务逻辑和引擎解耦的关键）
// ==========================================
// 定义：动作函数的输入是“数据包+长度”，输出是“下一个状态”
typedef OTA_State_t (*OTA_Action_t)(uint8_t *packet, uint16_t len);

// ==========================================
// 4. OTA上下文结构体（全局变量的“家”，所有状态数据存在这里）
// ==========================================
typedef struct {
    OTA_State_t current_state; // 当前状态（引擎用，判断当前在哪个状态）
    uint32_t firmware_size;    // 固件总大小（字节）
    uint32_t received_size;    // 已接收大小（字节，用于判断是否收完）
    uint16_t total_crc;        // 总CRC（预留，代码里没用到）
    uint32_t flash_offset;     // Flash写入偏移量（当前写到Flash的哪个地址了）
    
    // --- 【核心新增】蓄水池！解决250字节切片和256字节Flash页不对齐的问题 ---
    uint8_t  page_buffer[256]; // 256字节的“蓄水池”：先把切片填进来，凑够256再写Flash
    uint16_t buffer_idx;       // 蓄水池指针：当前蓄水池填了多少个字节
} OTA_Context_t;

extern OTA_Context_t ota_ctx; // 暴露全局上下文，给业务逻辑和引擎共用

// ==========================================
// 5. OTA数据包结构体（“快递盒”，装数据包）
// ==========================================
typedef struct {
    uint16_t length;    // 数据包长度
    uint8_t  payload[270]; // 数据包内容（270字节足够：2字节包头+1字节指令+2字节长度+256字节数据）
} OTA_Packet_t;

// ==========================================
// 6. 状态机微内核API声明
// ==========================================
void OTA_FSM_Init(void);                          // 初始化引擎
void OTA_FSM_ProcessPacket(uint8_t *packet, uint16_t len); // 处理数据包（核心入口）
uint8_t OTA_FSM_Register(OTA_State_t state, uint8_t event_cmd, OTA_Action_t action_func); // 动态注册状态
void OTA_Logic_Install(void);                     // 业务逻辑装载（统一注册所有动作）

#endif /* OTA_FSM_H */
