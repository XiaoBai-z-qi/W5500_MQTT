#include "monitor_task.h"
#include "main.h"
#include "debug_uart.h"

#define MAX_TASK_NUM 10   

extern TIM_HandleTypeDef htim5;
void TIM5_RunTimeStats_Init(void)
{
    HAL_TIM_Base_Start(&htim5);
}
uint32_t TIM5_Counter(void)
{
	return TIM5->CNT;
}

#define MAX_TASK_NUM 10

typedef struct
{
    TaskHandle_t handle;
    uint32_t lastRunTime;
} TaskRunTime_t;

static TaskRunTime_t taskInfo[MAX_TASK_NUM];
static uint32_t lastTotalRunTime = 0;

void TaskMonitor(void)
{
    TaskStatus_t taskStatusArray[MAX_TASK_NUM];
    UBaseType_t taskCount, i, j;
    uint32_t totalRunTime;

    taskCount = uxTaskGetSystemState(taskStatusArray, MAX_TASK_NUM, &totalRunTime);

    Debug_Printf("\r\n========== Task Monitor ==========\r\n");

    uint32_t totalDiff = totalRunTime - lastTotalRunTime;
    if (totalDiff == 0) totalDiff = 1;

    for (i = 0; i < taskCount; i++)
    {
        TaskHandle_t handle = taskStatusArray[i].xHandle;
        uint32_t current = taskStatusArray[i].ulRunTimeCounter;

        uint32_t last = 0;

        // 查找这个任务之前的记录
        for (j = 0; j < MAX_TASK_NUM; j++)
        {
            if (taskInfo[j].handle == handle)
            {
                last = taskInfo[j].lastRunTime;
                taskInfo[j].lastRunTime = current;
                break;
            }
        }

        // 新任务（第一次出现）
        if (j == MAX_TASK_NUM)
        {
            for (j = 0; j < MAX_TASK_NUM; j++)
            {
                if (taskInfo[j].handle == NULL)
                {
                    taskInfo[j].handle = handle;
                    taskInfo[j].lastRunTime = current;
                    break;
                }
            }
            last = current;
        }

        uint32_t diff = current - last;
        uint32_t cpu = (diff * 100UL) / totalDiff;

        uint32_t stack_free = taskStatusArray[i].usStackHighWaterMark;

        Debug_Printf("%-16s: CPU = %3lu%%, free_stack = %4lu\r\n",
                     taskStatusArray[i].pcTaskName,
                     cpu,
                     stack_free);
    }

    lastTotalRunTime = totalRunTime;
}
void vMonitorTask(void *argument)
{
    while (1)
    {
        TaskMonitor();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
