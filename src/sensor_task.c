#include "sensor_task.h"
#include "mpu6050.h"     /* your existing MPU6050 driver from Project 2 */
#include "stm32f4xx.h"   /* CMSIS header for GPIO access */

/* Inline GPIO toggle macros for minimum overhead */
#define SENSOR_PIN_HIGH()   (GPIOA->BSRR = (1U << SENSOR_TIMING_PIN))
#define SENSOR_PIN_LOW()    (GPIOA->BSRR = (1U << (SENSOR_TIMING_PIN + 16)))

void sensor_task(void *pvParameters) {
    (void)pvParameters;
    
    MPU6050DataPacket_t packet;
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS);
    
    /* Initialise xLastWakeTime with the current tick count.
     * This is the reference point for vTaskDelayUntil.
     * Must be called AFTER the scheduler starts. */
    xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        /* --- TIMING PIN HIGH: start of task work --- */
        SENSOR_PIN_HIGH();
        
        /* Read MPU6050 — 6 registers over I2C1 (reuse Project 2 driver) */
        mpu6050_read_raw(&packet.ax, &packet.ay, &packet.az,
                         &packet.gx, &packet.gy, &packet.gz);
        
        /* Timestamp: ticks since scheduler started */
        packet.timestamp = xTaskGetTickCount();
        
        /* Send to processing task via queue.
         * Timeout 0: non-blocking. If queue is full, drop the reading and
         * count it — we never want the highest-priority task to block on a
         * queue send. A queue depth of 10 at 50ms period gives 500ms buffer;
         * if the queue is ever full, the processing task has fallen 500ms
         * behind — a serious problem that should be logged, not waited on. */
        BaseType_t sent = xQueueSend(sensor_queue, &packet, 0);
        if (sent != pdTRUE) {
            /* Queue full — log drop, blink error LED, etc. */
            /* In production: increment a dropped_readings counter */
        }
        
        /* --- TIMING PIN LOW: end of task work --- */
        SENSOR_PIN_LOW();
        
        /* Block until the next period.
         * vTaskDelayUntil accounts for execution time — the period stays
         * exactly SENSOR_TASK_PERIOD_MS regardless of how long the work took. */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
    /* Tasks should never return. If they do, delete this task. */
    vTaskDelete(NULL);
}