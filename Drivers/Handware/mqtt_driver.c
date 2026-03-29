#include "mqtt_driver.h"
#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "DNS/dns.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug_uart.h"
#include "wizchip_port.h"
#include "jsmn_port.h"

#define ONENET_MQTT_VERSION     4

Network hnet1;
MQTTClient hclient1;

static uint8_t mqtt_sendbuf[MQTT_BUF_SIZE];
static uint8_t mqtt_readbuf[MQTT_BUF_SIZE];

static void messageArrived(MessageData *md);

int ONENET_MQTTNetworkInit(void)
{
    uint8_t broker_ip[4];
    uint8_t dns_server[4] = {114,114,114,114};

    Debug_Printf("=== ONENET MQTT START ===\r\n");
    int8_t ret = DNS_run(dns_server, (uint8_t*)ONENET_MQTT_HOST, broker_ip);
    if(ret != 1)
    {
        Debug_Printf("DNS failed : %d\r\n", ret);
        return -1;
    }
    Debug_Printf("ip : %d.%d.%d.%d\r\n", broker_ip[0], broker_ip[1], broker_ip[2], broker_ip[3]);

    /* 打开 TCP Socket */
    socket(MQTT_SOCKET, Sn_MR_TCP, 0, 0);
    Debug_Printf("[ONENET] Connecting to broker...\r\n");
    ret = connect(MQTT_SOCKET, broker_ip, ONENET_MQTT_PORT);
//    if(ret != SOCK_OK)
//    {
//        Debug_Printf("[ONENET] Connect failed: %d\r\n", ret);
//    }
    Debug_Printf("[ONENET] exit!!!\r\n");
    return 0;
}

int ONENET_MQTTConnent(MQTTClient *client, Network *net)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

    /* 初始化网络连接结构体 */
    NewNetwork(net, MQTT_SOCKET);

    /* 初始化 MQTT 客户端 */
    MQTTClientInit( client, net,
                    5000,                          //命令超时 (ms)
                    mqtt_sendbuf, MQTT_BUF_SIZE,
                    mqtt_readbuf, MQTT_BUF_SIZE);

    /* 配置连接参数 */
    data.MQTTVersion = ONENET_MQTT_VERSION;
    data.keepAliveInterval = ONENET_KEEPALIVE;
    data.cleansession = 1;
    data.clientID.cstring = ONENET_PRODUCT_ID;
    data.username.cstring = ONENET_DEVICE_NAME;
    data.password.cstring = ONENET_ACCESS_TOKEN;
    
    Debug_Printf("[OneNET] Connecting to MQTT broker...\r\n");
	//vTaskDelay(3000);
    if (MQTTConnect(client, &data) != SUCCESSS)
    {
        Debug_Printf("[OneNET] MQTT Connection FAILED!\r\n");
        return -1;
    }
    Debug_Printf("[OneNET] MQTT Connected successfully!\r\n");
    return 0;
}

/* 订阅主题 */
int ONENET_MQTTSubscribe(MQTTClient *client, const char *topic)
{
    if (!client->isconnected)
        return -1;
    int ret;
    ret = MQTTSubscribe(client, topic, QOS0, messageArrived);
    if (ret != SUCCESSS)
    {
        Debug_Printf("[OneNET] Subscribe failed: topic = %s, ret = %d\r\n", topic, ret);
        return -1;
    }
    
    Debug_Printf("[OneNET] Subscribed: topic = %s\r\n", topic);
    return 0;
}

/* 发布属性数据到 OneNET */
int ONENET_MQTTPublish(MQTTClient *client, const char *topic, const char *payload)
{
    if (!client->isconnected)
        return -1;
    MQTTMessage message;
    int ret;

    message.qos = QOS0;
    message.retained = 0;
    message.dup = 0;
    message.payload = (void*)payload;
    message.payloadlen = strlen(payload);

    ret = MQTTPublish(client, topic, &message);
    if(ret != SUCCESSS)
    {
        Debug_Printf("[OneNET] Publish failed: topic = %s, ret = %d\r\n", topic, ret);
        return -1;
    }
    Debug_Printf("[OneNET] Published: topic = %s\r\n", topic);
    return 0;
}

void ONENET_MQTTYield(MQTTClient *client, uint32_t timeout_ms)
{
    MQTTYield(client, timeout_ms);
}

/* --------------------------------------------------------------
 *  OneNET 属性设置消息处理回调（弱定义，用户可重写）
 * -------------------------------------------------------------- */
__weak void onenet_property_set_callback(const char *payload, int payload_len)
{
    int ret, id_num;
    char response[128];
    Debug_Printf("[OneNET] Property Set Received: %s\r\n", payload);
    jsmntok_t token[32];
    ret = json_parse(payload, token, 32);
    jsmntok_t *id_token = json_find_value(payload, token, ret, "id");
    id_num = json_token_to_int(payload, id_token);
    Debug_Printf("[OneNET] Property Set ID: %d\r\n", id_num);

    json_printf(response, sizeof(response), "{\"id\":\"%d\",\"code\":\"200\",\"msg\":\"success\"}", id_num);
    
    ONENET_MQTTPublish(&hclient1, replytopic, response);
}

/* --------------------------------------------------------------
 *  OneNET 命令下发消息处理回调（弱定义，用户可重写）
 * -------------------------------------------------------------- */
__weak void onenet_command_post_callback(const char *command_id, const char *payload, int payload_len)
{
    Debug_Printf("[OneNET] Command Post Received: ID=%s, Payload=%.*s\r\n", 
           command_id, payload_len, payload);
}

static void messageArrived(MessageData *md)
{
    char topic_buf[128];             //存储Topic名称，，最大64字节
    char payload_buf[256];          //用于存储 消息负载，最大长度 256 字节。
    int payload_len = md->message->payloadlen;
    
    memcpy(topic_buf, md->topicName->lenstring.data, 
           md->topicName->lenstring.len);
    topic_buf[md->topicName->lenstring.len] = '\0';
    
    if (payload_len >= sizeof(payload_buf))
        payload_len = sizeof(payload_buf) - 1;
    
    memcpy(payload_buf, md->message->payload, payload_len);
    payload_buf[payload_len] = '\0';
    
    Debug_Printf("\r\n[OneNET] Message Arrived on Topic: %s\r\n", topic_buf);
    Debug_Printf("[OneNET] Payload: %s\r\n", payload_buf);
    
    /* 判断 Topic 类型并调用相应回调 */
    if (strstr(topic_buf, "/thing/property/set") != NULL)
    {
        /* 属性设置消息 */
        onenet_property_set_callback(payload_buf, payload_len);
    }
    else if (strstr(topic_buf, "/thing/command/post") != NULL)
    {
        /* 命令下发消息 - 需要解析 command_id */
        /* 简化处理：直接传递整个 payload，用户自行解析 */
        onenet_command_post_callback("unknown", payload_buf, payload_len);
    }
}

/*=================================================================================*/
/*=================================================================================*/
/*=================================================================================*/

void ONENET_MQTT_Task(void *arg)
{
    uint8_t status = 0;
    if(W5500_Init() != 0)
        vTaskDelete(NULL);
    if(ONENET_MQTTNetworkInit() != 0)
        vTaskDelete(NULL);
//    if(ONENET_MQTTConnent(&hclient1, &hnet1) != 0)
//         vTaskDelete(NULL);
    if(ONENET_MQTTConnent(&hclient1, &hnet1) != 0)
			vTaskDelete(NULL);
	ONENET_MQTTSubscribe(&hclient1, subtopic);
    while(1)
    {
       ONENET_MQTTYield(&hclient1, 20);
       vTaskDelay(10);
    }
}

