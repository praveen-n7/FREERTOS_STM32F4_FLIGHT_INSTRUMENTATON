#include "processing_task.h"
#include "stm32f4xx.h"        /* GPIOA, BSRR — was missing */
#include <math.h>             /* sqrtf */

#define PROCESSING_PIN_HIGH()  (GPIOA->BSRR = (1U << PROCESSING_TIMING_PIN))
#define PROCESSING_PIN_LOW()   (GPIOA->BSRR = (1U << (PROCESSING_TIMING_PIN + 16)))

#define LPF_ALPHA   0.1f

void processing_task(void *pvParameters)
{
    (void)pvParameters;

    MPU6050DataPacket_t raw;
    ProcessedData_t result;
    float prev_filtered = 0.0f;

    for (;;)
    {
        /* Block indefinitely until sensor data arrives */
        xQueueReceive(sensor_queue, &raw, portMAX_DELAY);

        /* --- TIMING PIN HIGH --- */
        PROCESSING_PIN_HIGH();

        /* Compute acceleration magnitude using Cortex-M4F FPU */
        float ax_f = (float)raw.ax;
        float ay_f = (float)raw.ay;
        float az_f = (float)raw.az;
        result.magnitude = sqrtf(ax_f * ax_f + ay_f * ay_f + az_f * az_f);

        /* First-order IIR low-pass filter */
        result.filtered_mag = LPF_ALPHA * result.magnitude
                            + (1.0f - LPF_ALPHA) * prev_filtered;
        prev_filtered = result.filtered_mag;

        /* Preserve timestamp for latency measurement */
        result.timestamp = raw.timestamp;

        /* Send to display task */
        xQueueSend(display_queue, &result, pdMS_TO_TICKS(10));

        /* --- TIMING PIN LOW --- */
        PROCESSING_PIN_LOW();
    }
    vTaskDelete(NULL);
}