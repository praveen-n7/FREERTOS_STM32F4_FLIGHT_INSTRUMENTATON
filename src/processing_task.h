#ifndef PROCESSING_TASK_H
#define PROCESSING_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sensor_task.h"

typedef struct {
    float magnitude;        /* sqrt(ax^2 + ay^2 + az^2), in raw units */
    float filtered_mag;     /* low-pass filtered magnitude */
    TickType_t timestamp;   /* inherited from sensor packet for latency measurement */
} ProcessedData_t;

extern QueueHandle_t display_queue;

void processing_task(void *pvParameters);

#define PROCESSING_TASK_STACK_SIZE   256
#define PROCESSING_TASK_PRIORITY     2
#define PROCESSING_TIMING_PIN        5

#endif /* PROCESSING_TASK_H */