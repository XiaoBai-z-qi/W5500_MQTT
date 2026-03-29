#ifndef __MQTT_DRIVER_H__
#define __MQTT_DRIVER_H__
#include <stdint.h>
#include "MQTT/MQTTClient.h"
#include "MQTT/mqtt_interface.h"

#define MQTT_SOCKET             0       //MQTT 套接字
#define MQTT_BUF_SIZE           1024
#define ONENET_TOPIC_BUF_SIZE   128
#define ONENET_TOKEN_BUF_SIZE   128


#define ONENET_MQTT_HOST        "mqtts.heclouds.com"
#define ONENET_MQTT_PORT        1883
#define ONENET_KEEPALIVE        30

/* 设备信息配置 - 请根据实际设备修改 */
#define ONENET_PRODUCT_ID           "druk"
#define ONENET_DEVICE_NAME          "oK0brZsN6w"
#define ONENET_ACCESS_TOKEN         "version=2018-10-31&res=products%2FoK0brZsN6w%2Fdevices%2Fdruk&et=1781963724&method=md5&sign=sGoxHtyQbDoZlykukEqd2w%3D%3D"

#define pubtopic                    "$sys/oK0brZsN6w/druk/thing/property/post"
#define subtopic 					"$sys/oK0brZsN6w/druk/thing/property/set"
#define replytopic 					"$sys/oK0brZsN6w/druk/thing/property/set_reply"

int ONENET_MQTTNetworkInit(void);
int ONENET_MQTTConnent(MQTTClient *client, Network *net);
int ONENET_MQTTSubscribe(MQTTClient *client, const char *topic);
int ONENET_MQTTPublish(MQTTClient *client, const char *topic, const char *payload);
void ONENET_MQTTYield(MQTTClient *client, uint32_t timeout_ms);
void ONENET_MQTT_Task(void *arg);

#endif
