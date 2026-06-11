#ifndef ONENET_APP_H
#define ONENET_APP_H

#include <stdint.h>

void OneNET_Task(void *pvParameters);

#define WIFI_SSID       "jk"
#define WIFI_PWD        "12345678"

// ==================== OneNET MQTT 核心配置 ====================
#define ONENET_HOST         "mqtts.heclouds.com"
#define ONENET_PORT         1883

#define ONENET_PRODUCT_ID   "1Ccv2jnA25"
#define ONENET_DEVICE_ID    "esp"
#define ONENET_TOKEN        "version=2018-10-31&res=products%2F1Ccv2jnA25%2Fdevices%2Fesp&et=1805545509&method=md5&sign=eYJBjWtNcZf%2FRV7j6tZdxQ%3D%3D"


// ==================== OneNET HTTP OTA 核心配置 (新增) ====================
#define ONENET_HTTP_HOST    "iot-api.heclouds.com"
#define ONENET_HTTP_PORT    80
// 注意：HTTP的Token是userid级别的，和你上面设备级的MQTT Token不一样！
#define ONENET_HTTP_TOKEN   "version=2022-05-01&res=userid%2F471590&et=1793322172&method=sha1&sign=S6GRPNFl%2BVUlb3eSEdvrW%2Bfqi8Y%3D"

extern const char* MCU_VERSION_NOW; // 声明

// 假设你已经定义了 ONENET_PRODUCT_ID 和 ONENET_DEVICE_ID
#define ONENET_PUB_POST_TOPIC  "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_ID "/thing/property/post"

// 定义崩溃日志的属性标识符 (必须和你在 OneNET 网页上建的一模一样)
#define PROP_CRASH_LOG  "crash_log"



#endif /* ONENET_APP_H */
