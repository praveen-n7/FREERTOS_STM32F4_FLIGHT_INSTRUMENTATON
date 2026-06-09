#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <stdio.h>                /* snprintf */

#include "stm32f4xx.h"
#include "uart.h"                 /* uart2_init, uart2_send_string */
#include "i2c.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include "sensor_task.h"
#include "processing_task.h"
#include "display_task.h"
#include "priority_inversion_demo.h"

/* -------------------------------------------------------------------------
 * GLOBAL HANDLES
 * -------------------------------------------------------------------------*/
QueueHandle_t     sensor_queue;
QueueHandle_t     display_queue;
SemaphoreHandle_t spi_mutex;

TaskHandle_t sensor_task_handle;
TaskHandle_t processing_task_handle;
TaskHandle_t display_task_handle;

/* -------------------------------------------------------------------------
 * HARDWARE INITIALISATION
 * -------------------------------------------------------------------------*/
static void system_clock_init(void)
{
    /* Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Configure PLL: VCO = 8MHz/8 * 336 = 336MHz, SYSCLK = 336/2 = 168MHz */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE        |
                   (8U   << RCC_PLLCFGR_PLLM_Pos) |
                   (336U << RCC_PLLCFGR_PLLN_Pos) |
                   (0U   << RCC_PLLCFGR_PLLP_Pos) |
                   (7U   << RCC_PLLCFGR_PLLQ_Pos);

    /* Flash: 5 wait states, prefetch + caches */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS |
                 FLASH_ACR_PRFTEN      |
                 FLASH_ACR_ICEN        |
                 FLASH_ACR_DCEN;

    /* AHB/APB prescalers */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1   |
                RCC_CFGR_PPRE1_DIV4  |
                RCC_CFGR_PPRE2_DIV2;

    /* Enable PLL, wait for lock */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Switch to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static void timing_pins_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA4, PA5, PA6 as push-pull outputs — timing measurement pins */
    for (int pin = 4; pin <= 6; pin++) {
        GPIOA->MODER   &= ~(3U << (pin * 2));
        GPIOA->MODER   |=  (1U << (pin * 2));   /* output */
        GPIOA->OTYPER  &= ~(1U << pin);          /* push-pull */
        GPIOA->OSPEEDR |=  (3U << (pin * 2));    /* high speed */
        GPIOA->PUPDR   &= ~(3U << (pin * 2));    /* no pull */
    }
    /* All LOW at start */
    GPIOA->BSRR = (1U << (4 + 16)) | (1U << (5 + 16)) | (1U << (6 + 16));
}

/* -------------------------------------------------------------------------
 * MAIN
 * -------------------------------------------------------------------------*/
int main(void)
{
    /* 1. Hardware init */
    system_clock_init();

    /* Enable FPU — mandatory for Cortex-M4F floating point in tasks */
    SCB->CPACR |= (0xFU << 20);
    __DSB();
    __ISB();

    /* i2c1_init takes no arguments — speed is fixed at 400kHz in the driver */
    uart2_init(115200);
    i2c1_init();                  /* no argument — fixed 400kHz in i2c.c */
    mpu6050_init();
    ssd1306_init();
    timing_pins_init();

    uart2_send_string("Hardware init complete\r\n");

    /* 2. Create FreeRTOS objects */
    sensor_queue  = xQueueCreate(10, sizeof(MPU6050DataPacket_t));
    display_queue = xQueueCreate(5,  sizeof(ProcessedData_t));
    spi_mutex     = xSemaphoreCreateMutex();

    configASSERT(sensor_queue  != NULL);
    configASSERT(display_queue != NULL);
    configASSERT(spi_mutex     != NULL);

    /* 3. Create tasks */
    TaskHandle_t demo_handle;
    xTaskCreate(
        (TaskFunction_t)priority_inversion_demo_run,
        "PIDemo",
        512,
        NULL,
        4,
        &demo_handle
    );

    xTaskCreate(sensor_task,     "Sensor",
                SENSOR_TASK_STACK_SIZE,     NULL,
                SENSOR_TASK_PRIORITY,       &sensor_task_handle);

    xTaskCreate(processing_task, "Process",
                PROCESSING_TASK_STACK_SIZE, NULL,
                PROCESSING_TASK_PRIORITY,   &processing_task_handle);

    xTaskCreate(display_task,    "Display",
                DISPLAY_TASK_STACK_SIZE,    NULL,
                DISPLAY_TASK_PRIORITY,      &display_task_handle);

    uart2_send_string("Starting FreeRTOS scheduler...\r\n");

    /* 4. Start scheduler — never returns on success */
    vTaskStartScheduler();

    uart2_send_string("FATAL: vTaskStartScheduler returned!\r\n");
    while (1);
}

/* -------------------------------------------------------------------------
 * FREERTOS HOOK FUNCTIONS
 * -------------------------------------------------------------------------*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    char buf[64];
    snprintf(buf, sizeof(buf), "STACK OVERFLOW in task: %s\r\n", pcTaskName);
    uart2_send_string(buf);
    __disable_irq();
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    uart2_send_string("MALLOC FAILED — increase configTOTAL_HEAP_SIZE\r\n");
    __disable_irq();
    while (1);
}

void vApplicationIdleHook(void)
{
    /* __WFI(); — uncomment for sleep-on-idle */
}