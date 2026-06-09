#include "priority_inversion_demo.h"
#include "uart.h"
#include <stdio.h>

/* -------------------------------------------------------------------------
 * SHARED STATE
 * -------------------------------------------------------------------------*/
static SemaphoreHandle_t demo_sem;
static SemaphoreHandle_t demo_mutex;

static TaskHandle_t low_handle, med_handle, high_handle;

/* -------------------------------------------------------------------------
 * PHASE 1: Binary semaphore — BROKEN
 * -------------------------------------------------------------------------*/

static void demo_low_task_binary(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): taking binary semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    xSemaphoreTake(demo_sem, portMAX_DELAY);

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): GOT semaphore, working...\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    /* 2 seconds of busy work without releasing semaphore */
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(2000)) {
        __asm volatile("nop");
    }

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): releasing semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    xSemaphoreGive(demo_sem);

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): DONE\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    vTaskDelete(NULL);
}

static void demo_med_task_binary(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2): started, running freely\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) {
        __asm volatile("nop");
    }

    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2): DONE (blocked HighTask!)\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    vTaskDelete(NULL);
}

static void demo_high_task_binary(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    vTaskDelay(pdMS_TO_TICKS(10));

    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): trying semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    TickType_t blocked_at = xTaskGetTickCount();

    xSemaphoreTake(demo_sem, portMAX_DELAY);

    TickType_t unblocked_at = xTaskGetTickCount();

    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): GOT sem! Blocked %lu ms\r\n",
             (unsigned long)unblocked_at,
             (unsigned long)(unblocked_at - blocked_at));
    uart2_send_string(buf);

    uart2_send_string(">>> PRIORITY INVERSION DEMONSTRATED!\r\n");

    xSemaphoreGive(demo_sem);
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------
 * PHASE 2: Mutex — FIXED
 * -------------------------------------------------------------------------*/

static void demo_low_task_mutex(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): taking MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    xSemaphoreTake(demo_mutex, portMAX_DELAY);

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): GOT mutex, working...\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    /* Same 2 second busy loop
     * When HighTask blocks on this mutex FreeRTOS raises
     * our priority P1 to P3 automatically (priority inheritance)
     * MedTask at P2 cannot preempt us anymore */
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(2000)) {
        __asm volatile("nop");
    }

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): releasing MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    xSemaphoreGive(demo_mutex);

    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): DONE (ran at inherited P3)\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    vTaskDelete(NULL);
}

static void demo_med_task_mutex(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2): started\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) {
        __asm volatile("nop");
    }

    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2): blocked behind LowTask!\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    vTaskDelete(NULL);
}

static void demo_high_task_mutex(void *pvParameters)
{
    (void)pvParameters;
    char buf[64];

    vTaskDelay(pdMS_TO_TICKS(10));

    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): trying MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);

    TickType_t blocked_at = xTaskGetTickCount();

    /* Priority inheritance fires here automatically
     * LowTask priority raised P1 to P3
     * MedTask cannot preempt LowTask anymore */
    xSemaphoreTake(demo_mutex, portMAX_DELAY);

    TickType_t unblocked_at = xTaskGetTickCount();

    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): GOT mutex! Blocked %lu ms\r\n",
             (unsigned long)unblocked_at,
             (unsigned long)(unblocked_at - blocked_at));
    uart2_send_string(buf);

    uart2_send_string(">>> PRIORITY INHERITANCE FIXED IT!\r\n");

    xSemaphoreGive(demo_mutex);
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------
 * MAIN DEMO RUNNER
 * -------------------------------------------------------------------------*/

void priority_inversion_demo_run(void)
{
    uart2_send_string("\r\n========================================\r\n");
    uart2_send_string("PRIORITY INVERSION DEMONSTRATION\r\n");
    uart2_send_string("========================================\r\n\r\n");

    /* ---- PHASE 1: Binary semaphore ---- */
    uart2_send_string("--- PHASE 1: Binary Semaphore (BROKEN) ---\r\n");

    demo_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(demo_sem);

    xTaskCreate(demo_low_task_binary,  "DemoLow",  256, NULL, 1, &low_handle);
    xTaskCreate(demo_med_task_binary,  "DemoMed",  256, NULL, 2, &med_handle);
    xTaskCreate(demo_high_task_binary, "DemoHigh", 256, NULL, 3, &high_handle);

    vTaskDelay(pdMS_TO_TICKS(3500));

    /* ---- PHASE 2: Mutex ---- */
    uart2_send_string("\r\n--- PHASE 2: Mutex (FIXED) ---\r\n");

    demo_mutex = xSemaphoreCreateMutex();

    xTaskCreate(demo_low_task_mutex,  "DemoLow2",  256, NULL, 1, &low_handle);
    xTaskCreate(demo_med_task_mutex,  "DemoMed2",  256, NULL, 2, &med_handle);
    xTaskCreate(demo_high_task_mutex, "DemoHigh2", 256, NULL, 3, &high_handle);

    vTaskDelay(pdMS_TO_TICKS(3500));

    vSemaphoreDelete(demo_sem);
    vSemaphoreDelete(demo_mutex);

    uart2_send_string("\r\n========================================\r\n");
    uart2_send_string("DEMO COMPLETE - Starting sensor system\r\n");
    uart2_send_string("========================================\r\n\r\n");

    vTaskDelete(NULL);
}