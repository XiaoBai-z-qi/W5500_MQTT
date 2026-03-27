#ifndef __DEBUG_UART_H__
#define __DEBUG_UART_H__

#include "FreeRTOS.h"
#include "semphr.h"
#include "main.h"  // 添加以获取UART_HandleTypeDef定义

#define UART_TX_BUF_SIZE    1024  // 必须是2的幂

//xTaskCreate(UART_Debug_Task, "UART_Debug_Task", 256, NULL, configMAX_PRIORITIES - 1, NULL);

void UART_Debug_Init(UART_HandleTypeDef *huart);
int UART_Print(const char *fmt, ...);
int UART_Print_ISR(const char *str);
void UART_Debug_Task(void *pvParameters);
void Debug_UART_Callback(UART_HandleTypeDef *huart);


#endif
