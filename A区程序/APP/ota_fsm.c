#include "ota_fsm.h"
#include "usart.h"

OTA_Context_t ota_ctx; // 定义全局上下文（头文件里只是声明，这里才是定义）

// 动态注册表的最大容量（支持注册20个状态流转规则）
#define MAX_FSM_NODES 20

// 路由节点结构体（“规则表”的一行：当前状态+事件→动作）
typedef struct {
    OTA_State_t   state;       // 当前状态
    uint8_t       event_cmd;   // 事件指令字
    OTA_Action_t  action_func; // 对应的动作函数指针
} OTA_FSM_Node_t;

// 内存里的注册表（静态数组，但内容是动态注册的，外界看不到）
static OTA_FSM_Node_t m_fsm_list[MAX_FSM_NODES];
static uint8_t m_fsm_count = 0; // 当前注册了几个规则

// ==========================================
// 1. 引擎初始化
// ==========================================
void OTA_FSM_Init(void) {
    ota_ctx.current_state = OTA_STATE_IDLE; // 初始状态设为空闲
    ota_ctx.firmware_size = 0;
    ota_ctx.received_size = 0;
    m_fsm_count = 0; // 清空注册表
    
    // 【核心】装载所有业务逻辑！
    OTA_Logic_Install();
    
    u0_printf("[FSM-Engine] 内核启动！共挂载 %d 个状态路由。\r\n", m_fsm_count);
}

// ==========================================
// 2. 【终极杀招】动态注册状态（拒绝硬编码，避免Git冲突）
// ==========================================
uint8_t OTA_FSM_Register(OTA_State_t state, uint8_t event_cmd, OTA_Action_t action_func) {
    // 检查注册表满没满
    if (m_fsm_count >= MAX_FSM_NODES) {
        u0_printf("[FSM-Engine] ：注册表已满！\r\n");
        return 0; // 失败
    }
    // 检查动作函数是不是空指针
    if (action_func == NULL) return 0;

    // 【登记入册】把规则写进注册表
    m_fsm_list[m_fsm_count].state       = state;
    m_fsm_list[m_fsm_count].event_cmd   = event_cmd;
    m_fsm_list[m_fsm_count].action_func = action_func;
    m_fsm_count++;
    
    return 1; // 成功
}

// ==========================================
// 3. 【引擎运转核心】处理数据包：无情查表！
// ==========================================
void OTA_FSM_ProcessPacket(uint8_t *packet, uint16_t len) 
{
    // 3.1 先校验包头：必须是AA 55开头，长度至少3字节（AA+55+指令）
    if (len < 3 || packet[0] != 0xAA || packet[1] != 0x55) 
	{
	    u0_printf("[FSM-Engine] 包头校验失败！直接抛弃！\r\n");
        return; // 包头不对，直接丢包，不处理
	}		

    // 3.2 提取事件指令字：数据包的第3字节（索引2）
    uint8_t event_cmd = packet[2];

    // 3.3 【无情查表】遍历注册表，找“当前状态+事件”匹配的规则
    for (uint8_t i = 0; i < m_fsm_count; i++) {
        if (m_fsm_list[i].state == ota_ctx.current_state && 
            m_fsm_list[i].event_cmd == event_cmd) 
        {
            // 【匹配成功！】执行对应的动作函数，状态跃迁！
            ota_ctx.current_state = m_fsm_list[i].action_func(packet, len);
            return; // 执行完就返回，不继续查表
        }
    }
    // 3.4 没找到匹配的规则：非法事件，打印日志
    u0_printf("[FSM-Engine] 拦截非法事件: 0x%02X\r\n", event_cmd);
}
