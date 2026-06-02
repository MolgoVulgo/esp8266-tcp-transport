#ifndef TEST_FREERTOS_FREERTOS_H
#define TEST_FREERTOS_FREERTOS_H

#include <stdint.h>

typedef uint32_t portTickType;
typedef unsigned int UBaseType_t;
typedef void *xTaskHandle;
typedef int portBASE_TYPE;

#define portTICK_RATE_MS 1U
#define pdPASS 1

#endif
