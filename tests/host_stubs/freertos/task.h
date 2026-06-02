#ifndef TEST_FREERTOS_TASK_H
#define TEST_FREERTOS_TASK_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

xTaskHandle xTaskGetCurrentTaskHandle(void);
uint32_t xTaskGetTickCount(void);
void vTaskDelay(portTickType ticks);
void vTaskDelete(void *task);
UBaseType_t uxTaskGetStackHighWaterMark(void *task);
portBASE_TYPE xTaskCreate(void (*task_func)(void *), const char *name,
                          unsigned short stack_depth, void *param,
                          UBaseType_t priority, xTaskHandle *handle);

#ifdef __cplusplus
}
#endif

#endif
