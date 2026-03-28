#ifndef __MONITOR_TASK_H__
#define __MONITOR_TASK_H__
#include <stdint.h>
void TIM5_RunTimeStats_Init(void);
uint32_t TIM5_Counter(void);
#define configGENERATE_RUN_TIME_STATS 1
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()   TIM5_RunTimeStats_Init()
#define portGET_RUN_TIME_COUNTER_VALUE()           TIM5_Counter()

void TaskMonitor(void);
void vMonitorTask(void *argument);
#endif
