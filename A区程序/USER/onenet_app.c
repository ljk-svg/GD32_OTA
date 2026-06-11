#include "onenet_app.h"
#include "esp8266_at.h"
#include "hw_usart1.h"
#include "os_port.h"
#include "usart.h"
#include "ota_fsm.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "at24c02.h"
#include "gd32f10x.h"
#include "task_wdt.h"  // 引入看门狗服务
extern OSAL_Queue_t WIFI_MsgQueue; 
extern OSAL_Queue_t OTA_MsgQueue;  
static WIFI_Packet_t rx_pkt;

const char* MCU_VERSION_NOW = "V7.0"; // 以后改版本只改这里！

/* =========================================
 * 1. WiFi 连接函数 (从你之前调通的版本找回)
 * ========================================= */
static uint8_t WiFi_Connect(void)
{
    char cmd[128];

    
    u0_printf("[OneNET-App] 正在唤醒模块，清理残余状态...\r\n");
    OSAL_DelayMs(500); // 等待系统稳定
    
    // 如果它卡在透传或等待数据，用 +++ 和多发几个换行唤醒它
    USART1_SendString("+++"); 
    OSAL_DelayMs(200);
    USART1_SendString("\r\n\r\nAT\r\n");
    OSAL_DelayMs(500);
    
    // 把队列里刚才冲刷出来的乱七八糟的回应全部清空
    WIFI_Packet_t trash;
    while(OSAL_QueueReceive(WIFI_MsgQueue, &trash, 0));

    u0_printf("[OneNET-App] 正在检查 ESP8266 状态...\r\n");
    if(!ESP8266_SendCmd("AT\r\n", "OK", 1000)) return 0;
    
    u0_printf("[OneNET-App] 正在复位模块...\r\n");
    ESP8266_SendCmd("AT+RST\r\n", "ready", 3000);
    
	OSAL_DelayMs(500);
	while(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 0));
	  
    u0_printf("[OneNET-App] 设置为 STA 模式...\r\n");
    if(!ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 1000)) return 0;
    
    u0_printf("[OneNET-App] 正在连接 WiFi: %s ...\r\n", WIFI_SSID);
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PWD);
    if(!ESP8266_SendCmd(cmd, "WIFI GOT IP", 15000)) return 0;
    
    return 1;
}

/* =========================================
 * 2. MQTT 组包逻辑 (硬核二进制拼接)
 * ========================================= */
static uint16_t MQTT_BuildConnectPacket(uint8_t *pkt)
{
    uint16_t idx = 0;
    uint16_t client_id_len = strlen(ONENET_DEVICE_ID);
    uint16_t username_len = strlen(ONENET_PRODUCT_ID);
    uint16_t password_len = strlen(ONENET_TOKEN);
    uint16_t remain_len = 10 + (2 + client_id_len) + (2 + username_len) + (2 + password_len);

    pkt[idx++] = 0x10; // MQTT Connect 报文类型

    //  核心手术：标准 MQTT 动态长度编码算法！（破解 127 字节魔咒）
    do {
        uint8_t encodedByte = remain_len % 128;
        remain_len = remain_len / 128;
        // 如果还有数据，就把最高位（第7位）置 1
        if (remain_len > 0) {
            encodedByte |= 128;
        }
        pkt[idx++] = encodedByte;
    } while (remain_len > 0);

    // Variable header (10 bytes)
    pkt[idx++] = 0x00; pkt[idx++] = 0x04; 
    pkt[idx++] = 'M';  pkt[idx++] = 'Q';  pkt[idx++] = 'T';  pkt[idx++] = 'T';
    pkt[idx++] = 0x04; 
    pkt[idx++] = 0xC2; // 标志位：Clean Session, Username, Password
    pkt[idx++] = 0x00; pkt[idx++] = 0x3C; // Keep Alive: 60秒

    // Client ID
    pkt[idx++] = (client_id_len >> 8) & 0xFF; pkt[idx++] = client_id_len & 0xFF;
    memcpy(&pkt[idx], ONENET_DEVICE_ID, client_id_len); idx += client_id_len;

    // Username (Product ID)
    pkt[idx++] = (username_len >> 8) & 0xFF; pkt[idx++] = username_len & 0xFF;
    memcpy(&pkt[idx], ONENET_PRODUCT_ID, username_len); idx += username_len;

    // Password (Token)
    pkt[idx++] = (password_len >> 8) & 0xFF; pkt[idx++] = password_len & 0xFF;
    memcpy(&pkt[idx], ONENET_TOKEN, password_len); idx += password_len;

    return idx; // 返回真正的总长度
}
/* =========================================
 * 3. OneNET 登录逻辑
 * ========================================= */
static uint8_t OneNET_TCP_Connect(void)
{
    char cmd[128];
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ONENET_HOST, ONENET_PORT);
    return ESP8266_SendCmd(cmd, "CONNECT", 8000);
}

static uint8_t MQTT_Login(void)
{
    static uint8_t pkt[512]; 
    uint16_t len = MQTT_BuildConnectPacket(pkt);
    char cmd[32];
    WIFI_Packet_t res_pkt;

    u0_printf("[OneNET-App] 发送 MQTT 登录包...\r\n");
    sprintf(cmd, "AT+CIPSEND=%d\r\n", len);
    if(!ESP8266_SendCmd(cmd, ">", 2000)) return 0;

    USART1_SendData(pkt, len); // 发送二进制登录包

    // --- 核心修改：等待云端回复 CONNACK ---
    uint32_t wait = 0;
    while(wait < 5000) // 最多等 5 秒
    {
        if(OSAL_QueueReceive(WIFI_MsgQueue, &res_pkt, 100))
        {
            // 查找 MQTT CONNACK 报文固定头部 0x20
            for(int i=0; i < res_pkt.length; i++)
            {
                if(res_pkt.payload[i] == 0x20 && res_pkt.payload[i+1] == 0x02)
                {
                    if(res_pkt.payload[i+3] == 0x00) {
                        u0_printf("[OneNET-App] >>> 真正登录成功！收到 CONNACK <<<\r\n");
                        return 1;
                    } else {
                        u0_printf("[OneNET-App] 登录失败，错误码: %d\r\n", res_pkt.payload[i+3]);
                        return 0;
                    }
                }
            }
        }
        wait += 100;
    }
    u0_printf("[OneNET-App] 登录超时，云端没理我...\r\n");
    return 0;
}
/* =========================================
 * 新增：HTTP 透传 OTA 核心引擎
 * ========================================= */
static uint8_t OneNET_HTTP_OTA_Process(void)
{
	g_ota_running = 1;
	
   static char cmd[512];
   static char ota_tid[32] = {0};
    uint32_t ota_size = 0;
    WIFI_Packet_t rx_pkt;

    u0_printf("\r\n=== [HTTP OTA] 启动 OneNET HTTP OTA 流程 ===\r\n");

    // 1. TCP 连接 HTTP 服务器
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ONENET_HTTP_HOST, ONENET_HTTP_PORT);
    if(!ESP8266_SendCmd(cmd, "CONNECT", 8000)) {
        u0_printf("[HTTP OTA] 连接 HTTP 服务器失败！\r\n");
        return 0;
    }

    // 2. 开启透传模式 (暴力但有效，省去计算数据长度的烦恼)
    ESP8266_SendCmd("AT+CIPMODE=1\r\n", "OK", 1000);
    ESP8266_SendCmd("AT+CIPSEND\r\n", ">", 1000);
    u0_printf("[HTTP OTA] 成功进入透传模式！\r\n");

    // 清理一下历史包
    while(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 0));

   // 3. 上报版本号 (POST)
    // 完美复刻你网络助手的 41 字节格式（注意里面的空格）
char json_body[128];
// 使用你工程里定义的版本宏 MCU_VERSION_NOW
sprintf(json_body, "{\"s_version\":\"%s\", \"f_version\": \"%s\"}", MCU_VERSION_NOW, MCU_VERSION_NOW);

int real_len = strlen(json_body); 
u0_printf("[HTTP OTA] -> 动态上报当前版本: %s\r\n", MCU_VERSION_NOW);

    
    
    // 注意：Header 结尾的 \r\n\r\n 绝不能省
    sprintf(cmd, 
        "POST /fuse-ota/%s/%s/version HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Authorization:%s\r\n"
        "host:%s\r\n"
        "Content-Length:%d\r\n"
        "\r\n"
        "%s", 
        ONENET_PRODUCT_ID, ONENET_DEVICE_ID, ONENET_HTTP_TOKEN, ONENET_HTTP_HOST, 
        real_len, 
        json_body);

    USART1_SendString(cmd);

    // 等待 "msg":"succ" (最长等3秒)
    uint8_t wait_ok = 0;
    for(int i=0; i<30; i++) {
        if(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 100)) {
            rx_pkt.payload[rx_pkt.length] = '\0';
            
            // 【关键】把 HTTP 返回的原汁原味打印出来，不当瞎子！
            u0_printf("<< [POST-Raw]: %s\r\n", rx_pkt.payload); 
            
            if(strstr((char*)rx_pkt.payload, "\"msg\":\"succ\"")) {
                wait_ok = 1; 
                u0_printf("[HTTP OTA] 版本上报成功！\r\n");
                break;
            }
        }
    }
    if(!wait_ok) u0_printf("[HTTP OTA] 版本上报超时或失败！继续强行查询...\r\n");

    // 4. 查询任务 (GET)
    u0_printf("[HTTP OTA] -> 查询是否有新固件...\r\n");
    sprintf(cmd,
        "GET /fuse-ota/%s/%s/check?type=2&version=%s HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Authorization:%s\r\n"
        "host:%s\r\n\r\n",
        ONENET_PRODUCT_ID, ONENET_DEVICE_ID, MCU_VERSION_NOW, ONENET_HTTP_TOKEN, ONENET_HTTP_HOST);
    USART1_SendString(cmd);

    uint8_t has_task = 0;
    for(int i=0; i<50; i++) {
        if(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 100)) {
            rx_pkt.payload[rx_pkt.length] = '\0';
            
            if(strstr((char*)rx_pkt.payload, "\"msg\":\"not exist\"")) {
                u0_printf("[HTTP OTA] 结果: 当前版本已是最新，无更新任务。\r\n");
                break;
            }
            
            char *tid_ptr = strstr((char*)rx_pkt.payload, "\"tid\":");
            char *size_ptr = strstr((char*)rx_pkt.payload, "\"size\":");
            if(tid_ptr && size_ptr) {
                sscanf(tid_ptr, "\"tid\":%[^,]", ota_tid); // 提取 tid (到逗号为止)
                sscanf(size_ptr, "\"size\":%d", &ota_size); // 提取 size
                has_task = 1;
                u0_printf("[HTTP OTA] 发现新固件! TID: %s, 大小: %d Bytes\r\n", ota_tid, ota_size);
                break;
            }
        }
    }

    // 如果没有任务，退出透传，断开TCP，准备交给后面的MQTT！
    if(!has_task) {
        u0_printf("[HTTP OTA] 退出透传，断开 HTTP，准备切换到 MQTT...\r\n");
        OSAL_DelayMs(500);
        USART1_SendString("+++"); // 退出透传专属指令
        OSAL_DelayMs(1000);
        ESP8266_SendCmd("AT+CIPMODE=0\r\n", "OK", 1000);
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
        return 0; // 0代表无更新，继续走业务逻辑
    }

    // ================== 有任务，开始分片下载 ==================
    u0_printf("\r\n[HTTP OTA] 准备唤醒 FSM 状态机擦除 Flash...\r\n");
    OTA_Packet_t ota_cmd;
    ota_cmd.length = 9;
    ota_cmd.payload[0] = 0xAA; ota_cmd.payload[1] = 0x55; ota_cmd.payload[2] = 0x01; // START EVENT
    ota_cmd.payload[3] = (ota_size >> 24) & 0xFF;
    ota_cmd.payload[4] = (ota_size >> 16) & 0xFF;
    ota_cmd.payload[5] = (ota_size >> 8) & 0xFF;
    ota_cmd.payload[6] = (ota_size >> 0) & 0xFF;
    ota_cmd.payload[7] = 0; // CRC 等以后有需要再算
    ota_cmd.payload[8] = 0;
    
    // 把开始信号投喂给你写的完美状态机！
//    OSAL_QueueSend(OTA_MsgQueue, &ota_cmd, OSAL_WAIT_FOREVER);
//    // 给 FSM 一点时间去无情擦除 W25Q64
//    OSAL_DelayMs(1000); 
OTA_FSM_ProcessPacket(ota_cmd.payload, ota_cmd.length);
    // 开始 256 字节分片循环
    uint32_t offset = 0;
    uint8_t retry_count = 0;

    while(offset < ota_size) 
    {
        uint32_t end_byte = offset + 255;
        if(end_byte >= ota_size) end_byte = ota_size - 1; // 最后一包可能不足256
        uint32_t chunk_size = end_byte - offset + 1;

        sprintf(cmd,
            "GET /fuse-ota/%s/%s/%s/download HTTP/1.1\r\n"
            "Authorization:%s\r\n"
            "host:%s\r\n"
            "Range:bytes=%d-%d\r\n\r\n",
            ONENET_PRODUCT_ID, ONENET_DEVICE_ID, ota_tid, ONENET_HTTP_TOKEN, ONENET_HTTP_HOST, offset, end_byte);

        while(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 0)); // 清空防干扰
        USART1_SendString(cmd);

        // 5.1 跨越HTTP头部寻找空行 \r\n (这是你中断里 \n 切割的绝佳利用点！)
        uint8_t header_end = 0;
        for(int i=0; i<50; i++) {
            if(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 100)) {
                // 如果长度正好是 2 且 payload 是 \r\n，说明 HTTP 头部彻底结束，紧接着的全是纯净的二进制数据！
                if(rx_pkt.length == 2 && rx_pkt.payload[0] == '\r' && rx_pkt.payload[1] == '\n') {
                    header_end = 1; break;
                }
                // 容错：有些服务器回复只带 \n
                if(rx_pkt.length == 1 && rx_pkt.payload[0] == '\n') {
                    header_end = 1; break;
                }
            }
        }

        if(!header_end) {
            u0_printf("[HTTP OTA] 请求偏移 %d 超时，准备重试！\r\n", offset);
            if(++retry_count > 3) goto OTA_FAIL;
            continue;
        }

        // 5.2 收集纯净的 256 字节 (哪怕它被 USART 中断按 0x0A 切成了 10 份，我们也能拼回去)
        uint8_t bin_buf[256];
        uint32_t bin_idx = 0;
        uint8_t collect_ok = 1;

        while(bin_idx < chunk_size) {
            if(OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 300)) { // 300ms 收包超时
                uint32_t need = chunk_size - bin_idx;
                uint32_t copy_len = (rx_pkt.length <= need) ? rx_pkt.length : need;
                
                memcpy(&bin_buf[bin_idx], rx_pkt.payload, copy_len);
                bin_idx += copy_len;
            } else {
                u0_printf("[HTTP OTA] 二进制数据断流！已收 %d / 需 %d\r\n", bin_idx, chunk_size);
                collect_ok = 0; break;
            }
        }

        if(!collect_ok) {
            if(++retry_count > 3) goto OTA_FAIL;
            continue;
        }

        // 5.3 组装并投递给你牛逼的 FSM (走 Action_HandleData 进行烧录)
        ota_cmd.length = 5 + chunk_size;
        ota_cmd.payload[0] = 0xAA; ota_cmd.payload[1] = 0x55; ota_cmd.payload[2] = 0x02; // DATA EVENT
        ota_cmd.payload[3] = (chunk_size >> 8) & 0xFF;
        ota_cmd.payload[4] = chunk_size & 0xFF;
        memcpy(&ota_cmd.payload[5], bin_buf, chunk_size);

       // OSAL_QueueSend(OTA_MsgQueue, &ota_cmd, OSAL_WAIT_FOREVER);
		OTA_FSM_ProcessPacket(ota_cmd.payload, ota_cmd.length);

        offset += chunk_size;
        retry_count = 0;
        u0_printf("[HTTP OTA] 片段写入成功！总进度: %d / %d\r\n", offset, ota_size);
		WDT_Set_Alive(WDT_TASK_ONENET);
    }

    u0_printf("\r\n[HTTP OTA] 固件全部分片下载完毕！系统将在处理完毕后自刎重启！\r\n");
    while(1) OSAL_DelayMs(1000); // 挂起任务，死等 ota_logic 触发 CPU Reset

OTA_FAIL:
    u0_printf("[HTTP OTA] 极其惨烈：连续重试失败，退出透传，放弃 OTA。\r\n");
    OSAL_DelayMs(500); USART1_SendString("+++"); OSAL_DelayMs(1000);
    ESP8266_SendCmd("AT+CIPMODE=0\r\n", "OK", 1000);
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
    return 0;
}


/* =========================================
 * MQTT 订阅频道函数 (纯手工组包版)
 * ========================================= */
static uint8_t OneNET_MQTT_Subscribe(const char *topic)
{
    static uint8_t pkt[256];
    uint16_t topic_len = strlen(topic);
    
    // 剩余长度 = 报文标识符(2字节) + Topic长度占2字节 + Topic字符串本身 + QoS要求(1字节)
    uint16_t remain_len = 2 + (2 + topic_len) + 1; 
    uint16_t idx = 0;

    // 1. MQTT Subscribe 固定报头 (0x82)
    pkt[idx++] = 0x82; 

    // 2. 动态长度编码
    do {
        uint8_t encodedByte = remain_len % 128;
        remain_len = remain_len / 128;
        if (remain_len > 0) {
            encodedByte |= 128;
        }
        pkt[idx++] = encodedByte;
    } while (remain_len > 0);

    // 3. 报文标识符 Packet ID (随便写一个非零的，比如 0x00 0x0A)
    pkt[idx++] = 0x00;
    pkt[idx++] = 0x0A;

    // 4. 写入 Topic 的长度
    pkt[idx++] = (topic_len >> 8) & 0xFF;
    pkt[idx++] = topic_len & 0xFF;

    // 5. 写入 Topic 字符串
    memcpy(&pkt[idx], topic, topic_len);
    idx += topic_len;

    // 6. 写入 QoS 级别 (我们要求 QoS=0 即可)
    pkt[idx++] = 0x00;

    // 7. 发送给 ESP8266
    char cmd[32];
    sprintf(cmd, "AT+CIPSEND=%d\r\n", idx);
    
    if(ESP8266_SendCmd(cmd, ">", 2000)) 
    {
        USART1_SendData(pkt, idx); 
        u0_printf("[MQTT] 成功发送订阅请求，监听频道: %s\r\n", topic);
        OSAL_DelayMs(200); // 稍微等一下云端的 SUBACK 回复
        return 1;
    } 
    return 0;
}

/* =========================================
 * MQTT 业务数据发布函数
 * ========================================= */
static void OneNET_MQTT_Publish(const char *topic, const char *payload)
{
    static uint8_t pkt[512]; // 静态数组，省栈空间
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    
    // 剩余长度 = Topic长度占2字节 + Topic字符串本身 + Payload本身
    uint16_t remain_len = 2 + topic_len + payload_len;
    uint16_t idx = 0;

    // 1. MQTT Publish 固定报头 (0x30)
    pkt[idx++] = 0x30; 

    // 2. 核心：动态长度编码
    do {
        uint8_t encodedByte = remain_len % 128;
        remain_len = remain_len / 128;
        if (remain_len > 0) {
            encodedByte |= 128;
        }
        pkt[idx++] = encodedByte;
    } while (remain_len > 0);

    // 3. 写入 Topic 的长度 (高位在前，低位在后)
    pkt[idx++] = (topic_len >> 8) & 0xFF;
    pkt[idx++] = topic_len & 0xFF;

    // 4. 写入 Topic 字符串
    memcpy(&pkt[idx], topic, topic_len);
    idx += topic_len;

    // 5. 写入 Payload (真正的数据实体)
    memcpy(&pkt[idx], payload, payload_len);
    idx += payload_len;

    // 6. 唤醒 ESP8266 进入透传发送状态
    char cmd[32];
    sprintf(cmd, "AT+CIPSEND=%d\r\n", idx);
    
    // 等待 ">" 符号出现，说明 ESP8266 准备好接客了
    if(ESP8266_SendCmd(cmd, ">", 2000)) 
    {
        // 走你原有的发送二进制数据的底层函数！
        USART1_SendData(pkt, idx); 
        u0_printf("[MQTT] 消息已投递，长度: %d\r\n", idx);
    } 
    else 
    {
        u0_printf("[MQTT] 发送失败，ESP8266 未响应 > 符号。\r\n");
    }
}
/* =========================================
 * 4. 终极任务入口
 * ========================================= */




void OneNET_Task(void *pvParameters)
{
    


// 【重要修正】在主任务最开始，初始化 OTA 队列和挂载 FSM 引擎！
    if(OTA_MsgQueue == NULL) {
        OTA_MsgQueue = OSAL_QueueCreate(10, sizeof(OTA_Packet_t));
    }
    OTA_FSM_Init();

    u0_printf("\r\n[OneNET-App] 任务已启动，开始初始化硬件...\r\n");
	
	
    //  这里是核心连招，绝不跳过！
    if(!WiFi_Connect()) {
        u0_printf("[OneNET-App] WiFi 连接彻底失败，请检查接线和路由器！\r\n");
        while(1) OSAL_DelayMs(1000);
    }
    u0_printf("[OneNET-App] WiFi 已就绪！\r\n");

	
// 关键插入点：先检查 HTTP OTA！
    // 如果发现新任务，会在该函数内分片下载、写Flash然后强行重启。
    // 如果无任务，它会安全地退出透传和 TCP 连接，接着执行下面的 MQTT！
    OneNET_HTTP_OTA_Process();
    // =========================================================
	
    if(!OneNET_TCP_Connect()) {
        u0_printf("[OneNET-App] OneNET 服务器连接失败！\r\n");
        while(1) OSAL_DelayMs(1000);
    }
    u0_printf("[OneNET-App] TCP 已连接！\r\n");

    if(!MQTT_Login()) {
        u0_printf("[OneNET-App] MQTT 鉴权失败！请检查 Token！\r\n");
        while(1) OSAL_DelayMs(1000);
    }

    u0_printf("================================\r\n");
    u0_printf(" [OneNET-App] 登录成功！\r\n");
    u0_printf("================================\r\n");
	// =========================================================
    // 【必须做的一步】：订阅云端下发指令的 Topic
    // =========================================================
    char sub_topic[128];
    // 根据 OneNET 物模型规则，组装“属性设置”的下发频道
    sprintf(sub_topic, "$sys/%s/%s/thing/property/set", ONENET_PRODUCT_ID, ONENET_DEVICE_ID);
    
    // 发送订阅报文！
    OneNET_MQTT_Subscribe(sub_topic);
	
	// =========================================================
    // 【核心新增区】：诈尸检查与上报 (完美卡在登录后，死循环前)
    // =========================================================
   uint8_t crash_flag = 0;
    // 假设 0x10 是你用来存死亡标志的地址 (别和 OTA 的 0x00 冲突了)
    AT24C02_ReadData(0x10, &crash_flag, 1); 

    if (crash_flag == 0xAA) 
    {
        uint8_t pc_buf[4];
        AT24C02_ReadData(0x11, pc_buf, 4); 
        
        // 拼凑 32 位的 PC 地址
        uint32_t dead_pc = (pc_buf[0]<<24) | (pc_buf[1]<<16) | (pc_buf[2]<<8) | pc_buf[3];
        u0_printf("\r\n[Crash-Report] 发现死亡记录！死机 PC: 0x%08X\r\n", dead_pc);

        // 使用 snprintf 按照物模型格式组装 JSON
        char mqtt_payload[128];
        snprintf(mqtt_payload, sizeof(mqtt_payload), 
                 "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":\"HardFault_PC:0x%08X\"}}}", 
                 PROP_CRASH_LOG, dead_pc);

        // 调用刚才新写的底层组包函数发往云端！
        OneNET_MQTT_Publish(ONENET_PUB_POST_TOPIC, mqtt_payload);
        
        // 极其重要：销毁遗书，避免无限循环发送
        uint8_t clear_flag = 0x00;
        AT24C02_WriteBuffer(0x10, &clear_flag, 1);
        u0_printf("[Crash-Report] 遗书已安全销毁。\r\n");
    }
    while(1)
    {
		WDT_Set_Alive(WDT_TASK_ONENET);
        if (OSAL_QueueReceive(WIFI_MsgQueue, &rx_pkt, 1000) == 1)
        {
            rx_pkt.payload[rx_pkt.length] = '\0';
            u0_printf("[OneNET-RX] 收到云端指令: %s\r\n", rx_pkt.payload);

            // ==========================================================
            // 1. 暴力扫描：穿透 MQTT 的 0x00 截断，精准抓取 switch_bug
            // ==========================================================
            uint8_t trigger = 0;
            if (rx_pkt.length > 10) 
            {
                for(int i = 0; i < rx_pkt.length - 10; i++) 
                {
                    if(rx_pkt.payload[i]   == 's' &&
                       rx_pkt.payload[i+1] == 'w' &&
                       rx_pkt.payload[i+2] == 'i' &&
                       rx_pkt.payload[i+3] == 't' &&
                       rx_pkt.payload[i+4] == 'c' &&
                       rx_pkt.payload[i+5] == 'h' &&
                       rx_pkt.payload[i+6] == '_' &&
                       rx_pkt.payload[i+7] == 'b' &&
                       rx_pkt.payload[i+8] == 'u' &&
                       rx_pkt.payload[i+9] == 'g')
                    {
                        trigger = 1;
                        break; 
                    }
                }
            }

            // ==========================================================
            // 2. 自杀开关：清醒时写遗书，写完立刻复位
            // ==========================================================
            if (trigger)
            {
                u0_printf("\r\n!!! 触发自杀开关 (bug=ON)！准备记录 PC 并重启 !!!\r\n");
                OSAL_DelayMs(100); 

                // 趁着现在系统没死锁，先把遗书写好！
                uint8_t crash_flag = 0xAA; 
                AT24C02_WriteBuffer(0x10, &crash_flag, 1);
                
                // 伪造一个死机地址 (0x0800412C)
                uint8_t fake_pc[4] = {0x08, 0x00, 0x41, 0x2C};
                AT24C02_WriteBuffer(0x11, fake_pc, 4);

                // 留足 100ms 保证 EEPROM 烧写透彻
                OSAL_DelayMs(100); 

                u0_printf("遗书写入完毕，3秒后系统复位...\r\n");
                
                // 留3秒钟给你在串口看清上面那句话
                OSAL_DelayMs(3000); 

                // 硬件复位！
                NVIC_SystemReset(); 
            }
            
            // ==========================================================
            // 3. 升级相关 (这是你原本的代码，保留不动)
            // ==========================================================
            if (strstr((char*)rx_pkt.payload, "update") != NULL)
            {
                u0_printf(" 识别到升级关键字！投喂状态机...\r\n");
                OTA_Packet_t ota_start;
                ota_start.length = 9;
                ota_start.payload[0] = 0xAA; ota_start.payload[1] = 0x55; ota_start.payload[2] = 0x01;
                OSAL_QueueSend(OTA_MsgQueue, &ota_start, OSAL_WAIT_FOREVER);
            }
        }
    }

}


















