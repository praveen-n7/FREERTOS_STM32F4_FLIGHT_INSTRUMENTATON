#include "display_task.h"
#include "ssd1306.h"
#include "uart.h"
#include <stdio.h>

#define DISPLAY_PIN_HIGH()  (GPIOA->BSRR = (1U << DISPLAY_TIMING_PIN))
#define DISPLAY_PIN_LOW()   (GPIOA->BSRR = (1U << (DISPLAY_TIMING_PIN + 16)))

void display_task(void *pvParameters) {
    (void)pvParameters;
    
    ProcessedData_t data;
    char uart_buf[80];
    char line[22];  /* max 21 chars per line on SSD1306 at 6px/char */
    uint32_t frame_count = 0;
    
    for (;;) {
        /* Block until processed data is available */
        xQueueReceive(display_queue, &data, portMAX_DELAY);
        
        /* --- TIMING PIN HIGH --- */
        DISPLAY_PIN_HIGH();
        
        /* Take the shared bus mutex before using I2C (SSD1306 is on I2C).
         * 
         * Note: In this project, the SSD1306 uses I2C, not SPI. The mutex
         * concept is identical — we're protecting the I2C bus from concurrent
         * access. The variable is named spi_mutex to match the project
         * description which mentions SPI, but the protection pattern is the same.
         * If you add W25Q128 SPI flash logging from Project 4, this mutex
         * would protect that SPI bus instead.
         *
         * portMAX_DELAY: wait indefinitely for the mutex.
         * With priority inheritance: if SensorTask is also waiting for this
         * mutex (hypothetical), DisplayTask's priority gets raised automatically.
         */
        xSemaphoreTake(spi_mutex, portMAX_DELAY);
        
        /* Update OLED display with 4 lines of sensor data */
        ssd1306_clear();
        
        /* Line 0: Accel magnitude */
        snprintf(line, sizeof(line), "Mag:%6.1f", data.magnitude);
        ssd1306_set_cursor(0, 0);
        ssd1306_print_string(line);
        
        /* Line 1: Filtered magnitude */
        snprintf(line, sizeof(line), "Flt:%6.1f", data.filtered_mag);
        ssd1306_set_cursor(0, 2);
        ssd1306_print_string(line);
        
        /* Line 2: Tick timestamp */
        snprintf(line, sizeof(line), "T:%lu", (unsigned long)data.timestamp);
        ssd1306_set_cursor(0, 4);
        ssd1306_print_string(line);
        
        /* Line 3: Frame counter */
        snprintf(line, sizeof(line), "Frm:%lu", (unsigned long)frame_count++);
        ssd1306_set_cursor(0, 6);
        ssd1306_print_string(line);
        
        /* Push framebuffer to OLED — this takes ~20ms over I2C at 400kHz */
        ssd1306_update();
        
        /* Release the mutex immediately after we're done with I2C */
        xSemaphoreGive(spi_mutex);
        
        /* Also send over UART — no mutex needed, UART is only used by this task */
        snprintf(uart_buf, sizeof(uart_buf),
                 "[%5lu] Mag:%7.1f Flt:%7.1f\r\n",
                 (unsigned long)data.timestamp,
                 data.magnitude,
                 data.filtered_mag);
        uart2_send_string(uart_buf);
        
        /* Log stack high water marks every 100 frames for tuning */
        if (frame_count % 100 == 0) {
            snprintf(uart_buf, sizeof(uart_buf),
                     "[HEAP] Free: %u bytes\r\n",
                     (unsigned int)xPortGetFreeHeapSize());
            uart2_send_string(uart_buf);
        }
        
        /* --- TIMING PIN LOW --- */
        DISPLAY_PIN_LOW();
        
        /* No vTaskDelay — purely event-driven like ProcessingTask */
    }
    vTaskDelete(NULL);
}