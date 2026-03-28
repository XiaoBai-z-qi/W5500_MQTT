#include "debug_uart.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* ================= 配置 ================= */
#define UART_TX_BUF_SIZE 1024   // 必须是2的幂
#define UART_DMA_MAX_LEN 512

/* ================= 结构体 ================= */
typedef struct {
    uint8_t  buf[UART_TX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint8_t  busy;
} DebugTx_t;

static DebugTx_t dbg;

static UART_HandleTypeDef *uart;
static SemaphoreHandle_t sem;

/* 当前DMA发送长度（关键修复点） */
static uint16_t dma_len = 0;

/* ================= 内部函数 ================= */
static uint16_t buf_used(void)
{
    return (dbg.head - dbg.tail) & (UART_TX_BUF_SIZE - 1);
}

static uint16_t buf_free(void)
{
    return UART_TX_BUF_SIZE - 1 - buf_used();
}

/* 写入环形缓冲区 */
static void buf_write(const uint8_t *data, uint16_t len)
{
    uint16_t free_space = buf_free();

    if (len > free_space) {
        return;
    }

    for (uint16_t i = 0; i < len; i++) {
        dbg.buf[(dbg.head + i) & (UART_TX_BUF_SIZE - 1)] = data[i];
    }

    dbg.head = (dbg.head + len) & (UART_TX_BUF_SIZE - 1);
}

/* 启动DMA发送 */
static void dma_start(void)
{
    if (dbg.busy || buf_used() == 0)
        return;

    uint16_t tail = dbg.tail;
    uint16_t len;

    if (dbg.head >= tail) {
        len = dbg.head - tail;
    } else {
        len = UART_TX_BUF_SIZE - tail;
    }

    if (len > UART_DMA_MAX_LEN) {
        len = UART_DMA_MAX_LEN;
    }

    dbg.busy = 1;
    dma_len = len;   

    HAL_UART_Transmit_DMA(uart, &dbg.buf[tail], len);
}

/* ================= 回调 ================= */
void Debug_UART_Callback(UART_HandleTypeDef *huart)
{
    if(huart != uart)
        return;
    dbg.tail = (dbg.tail + dma_len) & (UART_TX_BUF_SIZE - 1);

    dbg.busy = 0;

    BaseType_t woken = pdFALSE;

    if (buf_used() > 0) {
        xSemaphoreGiveFromISR(sem, &woken);
    }

    portYIELD_FROM_ISR(woken);
}




/* ================= 任务 ================= */
void UART_Debug_Task(void *pvParameters)
{
	
    while(1)
    {
        xSemaphoreTake(sem, portMAX_DELAY);

        dma_start();
    }
}

/* ================= 初始化 ================= */
void UART_Debug_Init(UART_HandleTypeDef *huart)
{
    uart = huart;

    dbg.head = 0;
    dbg.tail = 0;
    dbg.busy = 0;

    sem = xSemaphoreCreateBinary();
    configASSERT(sem);
}

/* ================= 普通任务打印 ================= */
int UART_Print(const char *fmt, ...)
{
    char tmp[128];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n <= 0) return 0;
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;

    taskENTER_CRITICAL();
    buf_write((uint8_t *)tmp, n);
    taskEXIT_CRITICAL();

    xSemaphoreGive(sem);

    return n;
}

/* ================= ISR打印（超重要） ================= */
int UART_Print_ISR(const char *str)
{
    int len = strlen(str);

    BaseType_t woken = pdFALSE;

    /* ISR环境保护 */
    UBaseType_t mask = taskENTER_CRITICAL_FROM_ISR();

    buf_write((uint8_t *)str, len);

    taskEXIT_CRITICAL_FROM_ISR(mask);

    xSemaphoreGiveFromISR(sem, &woken);

    portYIELD_FROM_ISR(woken);

    return len;
}

/* ================= Flush ================= */
void UART_Flush(void)
{
    while (buf_used() > 0 || dbg.busy) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
