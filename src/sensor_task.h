#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    TickType_t timestamp;
} MPU6050DataPacket_t;

/* Queue handle — defined in main.c, declared extern here */
extern QueueHandle_t sensor_queue;

/* Task function */
void sensor_task(void *pvParameters);

/* Task parameters */
#define SENSOR_TASK_STACK_SIZE    256   /* words = 1024 bytes */
#define SENSOR_TASK_PRIORITY      3
#define SENSOR_TASK_PERIOD_MS     50

/* Timing GPIO — PA4 */
#define SENSOR_TIMING_PIN         4

#endif /* SENSOR_TASK_H */