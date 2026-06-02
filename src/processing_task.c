#include "processing_task.h"
#include <math.h>      /* sqrtf — uses Cortex-M4F FPU instruction, ~20 cycles */

#define PROCESSING_PIN_HIGH()  (GPIOA->BSRR = (1U << PROCESSING_TIMING_PIN))
#define PROCESSING_PIN_LOW()   (GPIOA->BSRR = (1U << (PROCESSING_TIMING_PIN + 16)))

/* Low-pass filter coefficient: alpha = 0.1
 * filtered = alpha * new + (1 - alpha) * previous
 * Higher alpha = less filtering, faster response
 * Lower alpha = more filtering, smoother output */
#define LPF_ALPHA   0.1f

void processing_task(void *pvParameters) {
    (void)pvParameters;
    
    MPU6050DataPacket_t raw;
    ProcessedData_t result;
    float prev_filtered = 0.0f;  /* low-pass filter state */
    
    /* MPU6050 at ±2g range: raw value / 16384.0 = g's
     * We keep raw units for simplicity, convert only if needed for display */
    
    for (;;) {
        /* Block indefinitely until sensor data arrives.
         * portMAX_DELAY = 0xFFFFFFFF ticks = practically forever.
         * This task consumes ZERO CPU while waiting.
         * As soon as SensorTask sends a packet, this task unblocks. */
        xQueueReceive(sensor_queue, &raw, portMAX_DELAY);
        
        /* --- TIMING PIN HIGH --- */
        PROCESSING_PIN_HIGH();
        
        /* Compute acceleration magnitude — Cortex-M4F FPU handles sqrtf natively */
        float ax_f = (float)raw.ax;
        float ay_f = (float)raw.ay;
        float az_f = (float)raw.az;
        result.magnitude = sqrtf(ax_f * ax_f + ay_f * ay_f + az_f * az_f);
        
        /* Apply first-order IIR low-pass filter */
        result.filtered_mag = LPF_ALPHA * result.magnitude 
                              + (1.0f - LPF_ALPHA) * prev_filtered;
        prev_filtered = result.filtered_mag;
        
        /* Preserve original timestamp for latency measurement.
         * On the logic analyser: time from SensorTask PA4 falling to
         * ProcessingTask PA5 rising = queue latency (should be 0-1ms) */
        result.timestamp = raw.timestamp;
        
        /* Send to display task. Timeout 10ms: if display queue is full,
         * we wait briefly. Display task is slow (OLED update ~20ms) but
         * queue depth 5 gives 5 frames of buffering. */
        xQueueSend(display_queue, &result, pdMS_TO_TICKS(10));
        
        /* --- TIMING PIN LOW --- */
        PROCESSING_PIN_LOW();
        
        /* No vTaskDelay here — this task is purely event-driven.
         * It runs exactly as fast as data arrives from SensorTask.
         * It will naturally run at the same rate as SensorTask (every 50ms). */
    }
    vTaskDelete(NULL);
}