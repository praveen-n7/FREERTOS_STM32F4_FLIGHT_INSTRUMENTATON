#include "priority_inversion_demo.h"
#include "uart.h"
#include <stdio.h>

/* -------------------------------------------------------------------------
 * SHARED STATE for the demonstration
 * -------------------------------------------------------------------------*/
static SemaphoreHandle_t demo_sem;   /* binary semaphore — the BROKEN version */
static SemaphoreHandle_t demo_mutex; /* mutex — the FIXED version */
static volatile int demo_phase;      /* 1 = binary sem, 2 = mutex */

/* Task handles so we can delete them after each phase */
static TaskHandle_t low_handle, med_handle, high_handle;

/* All demo tasks use UART — no mutex on UART since these tasks
 * are running at strictly different times (never concurrent UART writes
 * in this specific demo). In production, UART would also need protection. */

/* -------------------------------------------------------------------------
 * PHASE 1: Binary semaphore version — DEMONSTRATES THE BUG
 * -------------------------------------------------------------------------*/

static void demo_low_task_binary(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    /* LowTask takes the binary semaphore immediately */
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): taking binary semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    xSemaphoreTake(demo_sem, portMAX_DELAY);
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): GOT semaphore, doing 2 sec of work\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    /* Simulate 2 seconds of work WITHOUT releasing the semaphore.
     * We use a busy-loop here to consume CPU and demonstrate the
     * preemption behaviour correctly. vTaskDelay would block and let
     * other tasks run, which wouldn't show the inversion properly.
     *
     * At 168MHz, 168000000 iterations ≈ 1 second (rough — depends on loop
     * compilation). We use tick counting for accuracy. */
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(2000)) {
        /* busy loop — consumes 100% CPU at this priority level */
        __asm volatile("nop");
    }
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): releasing binary semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    xSemaphoreGive(demo_sem);
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): DONE\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    vTaskDelete(NULL);
}

static void demo_med_task_binary(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    /* MediumTask has no semaphore dependency.
     * It should NOT be able to block HighTask.
     * But with binary semaphore (no priority inheritance), it CAN and DOES. */
    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2):  started, running freely\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    /* Run for 500ms to demonstrate it can steal CPU from LowTask
     * (and thus indirectly from HighTask which is waiting for LowTask) */
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) {
        __asm volatile("nop");
    }
    
    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2):  DONE (ran freely, blocking HighTask!)\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    vTaskDelete(NULL);
}

static void demo_high_task_binary(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    /* HighTask waits 10ms to let LowTask grab the semaphore first */
    vTaskDelay(pdMS_TO_TICKS(10));
    
    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): trying to take binary semaphore\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    TickType_t blocked_at = xTaskGetTickCount();
    
    xSemaphoreTake(demo_sem, portMAX_DELAY);  /* BLOCKED — LowTask holds it */
    
    TickType_t unblocked_at = xTaskGetTickCount();
    
    snprintf(buf, sizeof(buf),
             "[%5lu] HighTask(P3): GOT semaphore! Was blocked for %lu ms\r\n"
             "       >>> PRIORITY INVERSION: HighTask blocked while MedTask ran freely!\r\n",
             (unsigned long)unblocked_at,
             (unsigned long)(unblocked_at - blocked_at));
    uart2_send_string(buf);
    
    xSemaphoreGive(demo_sem);
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------
 * PHASE 2: Mutex version — DEMONSTRATES THE FIX
 * -------------------------------------------------------------------------*/

static void demo_low_task_mutex(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): taking MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    xSemaphoreTake(demo_mutex, portMAX_DELAY);
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): GOT mutex, doing 2 sec of work\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    /* Same 2-second busy loop as before */
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(2000)) {
        /* When HighTask blocks on this mutex, FreeRTOS will RAISE our priority
         * from P1 to P3 (priority inheritance). MedTask at P2 cannot preempt us. */
        __asm volatile("nop");
    }
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): releasing MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    xSemaphoreGive(demo_mutex);
    
    snprintf(buf, sizeof(buf), "[%5lu] LowTask(P1): DONE (ran at inherited priority P3)\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    vTaskDelete(NULL);
}

static void demo_med_task_mutex(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2):  started\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) {
        __asm volatile("nop");
        /* LowTask is now running at inherited P3 — we at P2 cannot preempt it */
    }
    
    snprintf(buf, sizeof(buf), "[%5lu] MedTask(P2):  NOTE: I was blocked behind LowTask!\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    vTaskDelete(NULL);
}

static void demo_high_task_mutex(void *pvParameters) {
    (void)pvParameters;
    char buf[80];
    
    /* Same 10ms delay to let LowTask grab the mutex first */
    vTaskDelay(pdMS_TO_TICKS(10));
    
    snprintf(buf, sizeof(buf), "[%5lu] HighTask(P3): trying to take MUTEX\r\n",
             (unsigned long)xTaskGetTickCount());
    uart2_send_string(buf);
    
    TickType_t blocked_at = xTaskGetTickCount();
    
    /* FreeRTOS priority inheritance fires here:
     * When we block on the mutex held by LowTask(P1),
     * FreeRTOS raises LowTask's priority to P3.
     * MedTask(P2) can no longer preempt LowTask.
     * LowTask finishes faster, giving us the mutex sooner. */
    xSemaphoreTake(demo_mutex, portMAX_DELAY);
    
    TickType_t unblocked_at = xTaskGetTickCount();
    
    snprintf(buf, sizeof(buf),
             "[%5lu] HighTask(P3): GOT mutex! Was blocked for %lu ms\r\n"
             "       >>> FIXED: LowTask ran at inherited P3, MedTask could not preempt!\r\n",
             (unsigned long)unblocked_at,
             (unsigned long)(unblocked_at - blocked_at));
    uart2_send_string(buf);
    
    xSemaphoreGive(demo_mutex);
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------
 * MAIN DEMO RUNNER — called from main.c before sensor system starts
 * -------------------------------------------------------------------------*/

void priority_inversion_demo_run(void) {
    uart2_send_string("\r\n========================================\r\n");
    uart2_send_string("PRIORITY INVERSION DEMONSTRATION\r\n");
    uart2_send_string("========================================\r\n\r\n");
    
    /* ---- PHASE 1: Binary semaphore — broken ---- */
    uart2_send_string("--- PHASE 1: Binary Semaphore (BROKEN) ---\r\n");
    uart2_send_string("Expected: HighTask(P3) blocked for ~2000ms while MedTask runs\r\n\r\n");
    
    demo_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(demo_sem);  /* make it available */
    
    /* Create all three tasks simultaneously.
     * LowTask at P1 starts first (lower priority, but we create it first
     * and add a brief yield so it runs and takes the semaphore before
     * HighTask tries to take it). HighTask has a 10ms internal delay. */
    xTaskCreate(demo_low_task_binary,  "DemoLow",  256, NULL, 1, &low_handle);
    xTaskCreate(demo_med_task_binary,  "DemoMed",  256, NULL, 2, &med_handle);
    xTaskCreate(demo_high_task_binary, "DemoHigh", 256, NULL, 3, &high_handle);
    
    /* Wait for phase 1 to complete (~3 seconds) */
    vTaskDelay(pdMS_TO_TICKS(3500));
    
    uart2_send_string("\r\n--- PHASE 2: Mutex (FIXED) ---\r\n");
    uart2_send_string("Expected: HighTask(P3) blocked for ~2000ms BUT MedTask cannot interfere\r\n\r\n");
    
    demo_mutex = xSemaphoreCreateMutex();
    
    xTaskCreate(demo_low_task_mutex,  "DemoLow2",  256, NULL, 1, &low_handle);
    xTaskCreate(demo_med_task_mutex,  "DemoMed2",  256, NULL, 2, &med_handle);
    xTaskCreate(demo_high_task_mutex, "DemoHigh2", 256, NULL, 3, &high_handle);
    
    /* Wait for phase 2 to complete */
    vTaskDelay(pdMS_TO_TICKS(3500));
    
    vSemaphoreDelete(demo_sem);
    vSemaphoreDelete(demo_mutex);
    
    uart2_send_string("\r\n========================================\r\n");
    uart2_send_string("DEMO COMPLETE — Starting sensor system\r\n");
    uart2_send_string("========================================\r\n\r\n");
}