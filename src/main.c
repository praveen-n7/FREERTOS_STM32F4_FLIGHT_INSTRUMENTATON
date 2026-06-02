#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "stm32f4xx.h"
#include "uart.h"
#include "i2c.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include "sensor_task.h"
#include "processing_task.h"
#include "display_task.h"
#include "priority_inversion_demo.h"

/* -------------------------------------------------------------------------
 * GLOBAL HANDLES — defined here, extern in individual task headers
 * -------------------------------------------------------------------------*/
QueueHandle_t sensor_queue;
QueueHandle_t display_queue;
SemaphoreHandle_t spi_mutex;

/* Task handles — needed for uxTaskGetStackHighWaterMark reporting */
TaskHandle_t sensor_task_handle;
TaskHandle_t processing_task_handle;
TaskHandle_t display_task_handle;

/* -------------------------------------------------------------------------
 * HARDWARE INITIALISATION
 * -------------------------------------------------------------------------*/
static void system_clock_init(void) {
    /* Your existing 168MHz PLL setup from Projects 1-6.
     * HSE (8MHz on Discovery) → PLL → 168MHz SYSCLK
     * Flash latency 5 WS, AHB=168MHz, APB1=42MHz, APB2=84MHz */
    
    /* Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    
    /* Configure PLL: VCO = 8MHz/8 * 336 = 336MHz, SYSCLK = 336/2 = 168MHz */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE |
                   (8  << RCC_PLLCFGR_PLLM_Pos) |   /* /8 */
                   (336 << RCC_PLLCFGR_PLLN_Pos) |  /* x336 */
                   (0  << RCC_PLLCFGR_PLLP_Pos) |   /* /2 */
                   (7  << RCC_PLLCFGR_PLLQ_Pos);    /* USB: 336/7=48MHz */
    
    /* Flash: 5 wait states for 168MHz, enable prefetch + caches */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS |
                 FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN;
    
    /* AHB/APB prescalers */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 |    /* AHB = 168MHz */
                RCC_CFGR_PPRE1_DIV4 |   /* APB1 = 42MHz */
                RCC_CFGR_PPRE2_DIV2;    /* APB2 = 84MHz */
    
    /* Enable PLL, wait for lock */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));
    
    /* Switch to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static void timing_pins_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    
    /* PA4, PA5, PA6 as outputs — see Section 6 for full code */
    for (int pin = 4; pin <= 6; pin++) {
        GPIOA->MODER  &= ~(3U << (pin*2));
        GPIOA->MODER  |=  (1U << (pin*2));
        GPIOA->OTYPER &= ~(1U << pin);
        GPIOA->OSPEEDR|=  (3U << (pin*2));
        GPIOA->PUPDR  &= ~(3U << (pin*2));
    }
    GPIOA->BSRR = (1<<(4+16)) | (1<<(5+16)) | (1<<(6+16));  /* all LOW */
}

/* -------------------------------------------------------------------------
 * MAIN
 * -------------------------------------------------------------------------*/
int main(void) {
    /* 1. Hardware init — must happen before FreeRTOS */
    system_clock_init();
    
    /* Enable FPU — CRITICAL for Cortex-M4F floating-point in tasks */
    SCB->CPACR |= (0xF << 20);   /* CP10 and CP11 full access */
    __DSB();
    __ISB();
    
    uart2_init(115200);
    i2c1_init(400000);   /* 400kHz fast mode for MPU6050 + SSD1306 */
    mpu6050_init();
    ssd1306_init();
    timing_pins_init();
    
    uart2_send_string("Hardware init complete\r\n");
    
    /* 2. Create FreeRTOS objects — queues and mutexes must be created
     *    BEFORE creating tasks that use them */
    sensor_queue  = xQueueCreate(10, sizeof(MPU6050DataPacket_t));
    display_queue = xQueueCreate(5,  sizeof(ProcessedData_t));
    spi_mutex     = xSemaphoreCreateMutex();
    
    /* Sanity check — if any creation fails, heap is too small */
    configASSERT(sensor_queue  != NULL);
    configASSERT(display_queue != NULL);
    configASSERT(spi_mutex     != NULL);
    
    /* 3. Create tasks */
    /* Demo task runs FIRST — it creates its own subtasks internally.
     * We create it at high priority so it runs before sensor tasks.
     * It will delete itself when done. */
    TaskHandle_t demo_handle;
    xTaskCreate(
        (TaskFunction_t)priority_inversion_demo_run,
        "PIDemo",
        512,    /* larger stack — demo creates subtasks internally */
        NULL,
        4,      /* highest priority — runs first before everything else */
        &demo_handle
    );
    
    /* Sensor task — priority 3, reads MPU6050 every 50ms */
    xTaskCreate(sensor_task, "Sensor",
                SENSOR_TASK_STACK_SIZE, NULL,
                SENSOR_TASK_PRIORITY, &sensor_task_handle);
    
    /* Processing task — priority 2, event-driven by sensor queue */
    xTaskCreate(processing_task, "Process",
                PROCESSING_TASK_STACK_SIZE, NULL,
                PROCESSING_TASK_PRIORITY, &processing_task_handle);
    
    /* Display task — priority 1, event-driven by display queue */
    xTaskCreate(display_task, "Display",
                DISPLAY_TASK_STACK_SIZE, NULL,
                DISPLAY_TASK_PRIORITY, &display_task_handle);
    
    uart2_send_string("Starting FreeRTOS scheduler...\r\n");
    
    /* 4. Start scheduler — this call never returns on success.
     *    If it returns, heap was too small to create the idle task. */
    vTaskStartScheduler();
    
    /* Should never reach here */
    uart2_send_string("FATAL: vTaskStartScheduler returned!\r\n");
    while (1);
}

/* -------------------------------------------------------------------------
 * FREERTOS HOOK FUNCTIONS — must be implemented when hooks are enabled
 * -------------------------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    char buf[64];
    snprintf(buf, sizeof(buf), "STACK OVERFLOW in task: %s\r\n", pcTaskName);
    uart2_send_string(buf);
    __disable_irq();
    while (1);
}

void vApplicationMallocFailedHook(void) {
    uart2_send_string("MALLOC FAILED — heap exhausted! Increase configTOTAL_HEAP_SIZE\r\n");
    __disable_irq();
    while (1);
}

/* Idle hook — called by idle task each cycle.
 * Use for power saving (WFI) in production. Keep it fast and non-blocking. */
void vApplicationIdleHook(void) {
    /* __WFI(); — uncomment for sleep-on-idle in battery-powered applications */
}