#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "processing_task.h"

extern SemaphoreHandle_t spi_mutex;  /* shared bus protection */

void display_task(void *pvParameters);

#define DISPLAY_TASK_STACK_SIZE   512   /* larger: calls ssd1306_update() → I2C */
#define DISPLAY_TASK_PRIORITY     1
#define DISPLAY_TIMING_PIN        6

#endif /* DISPLAY_TASK_H */