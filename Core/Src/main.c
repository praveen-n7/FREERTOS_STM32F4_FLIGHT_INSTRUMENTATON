#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"


#include "board_init.h"
#include "delay.h"
#include "i2c_bus.h"
#include "mpu6050.h"
#include "bmp280.h"
#include "flight_core.h"
#include "ssd1306.h"

#include <stdint.h>
#include <stdio.h>


SemaphoreHandle_t i2cMutex;
QueueHandle_t Raw_Data_Queue;
QueueHandle_t Display_Queue;


TaskHandle_t sensor;
TaskHandle_t fusion;
TaskHandle_t display;


extern Attitude_t current_attitude;


void SensorTask(void *argument);
void FusionTask(void *argument);
void DisplayTask(void *argument);


int main(void)
{

    boardInit();
    delayInit();

    i2cConfig();
    delay_ms(50);


    ssd1306_Init();
    ssd1306_Fill(Black);

    ssd1306_SetCursor(10, 10);
    ssd1306_WriteString("INITIALIZING...", Font_7x10, White);
    ssd1306_SetCursor(10, 30);
    ssd1306_WriteString("DO NOT MOVE!", Font_7x10, White);
    ssd1306_UpdateScreen();

    delay_ms(1000);



    mpu6050_init();
    delay_ms(100);

    ssd1306_Fill(Black);
    ssd1306_SetCursor(10, 20);
    ssd1306_WriteString("CALIBRATING...", Font_7x10, White);
    ssd1306_UpdateScreen();

    mpu6050_calibrate_gyro();



    bmp280_init();
    bmp280_calibrate_basic_altitude();



    ssd1306_Fill(Black);
    ssd1306_SetCursor(5, 10);
    ssd1306_WriteString("SYSTEM READY!", Font_7x10, White);
    ssd1306_SetCursor(0, 30);
    ssd1306_WriteString("READY FOR TAKEOFF!", Font_7x10, White);
    ssd1306_UpdateScreen();
    delay_ms(1000);



    i2cMutex = xSemaphoreCreateMutex();
    Raw_Data_Queue = xQueueCreate(5, sizeof(RawData_t));
    Display_Queue  = xQueueCreate(1, sizeof(Attitude_t));

    xTaskCreate(DisplayTask, "OLED_Screen",  1024, NULL, 3, &display);
    xTaskCreate(SensorTask,  "ReadSensors",  512,  NULL, 2, &sensor);
    xTaskCreate(FusionTask,  "SensorFusion", 512,  NULL, 1, &fusion);


    vTaskStartScheduler();

    while(1) { }
}

void SensorTask(void *argument)
{
    RawData_t localData;
    uint32_t now;
    int baro_divider = 0;
    float raw_altitude = 0.0f;
    static float filtered_altitude = 0.0f;
    const TickType_t xFrequency = pdMS_TO_TICKS(4); // 250Hz
    TickType_t xLastWakeTime;

    xLastWakeTime = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(100));

    while(1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if(xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
        {
            now = millis();
            localData.timestamp = now;

            // Read accel and gyro directly — no INT_STATUS check needed.
            // The MPU6050 runs in Normal mode at 250Hz (set by DLPF+SMPLRT),
            // data registers are always valid; polling INT_STATUS only matters
            // if the INT pin is wired, which it may not be.
            if(mpu6050_read_accel(&localData.ax, &localData.ay, &localData.az) == 0 &&
               mpu6050_read_gyro(&localData.gx, &localData.gy, &localData.gz) == 0)
            {
                baro_divider++;

                if(baro_divider >= 10)
                {
                    baro_divider = 0;

                    raw_altitude = read_altitude();

                    if(filtered_altitude == 0.0f)
                    {
                        filtered_altitude = raw_altitude;
                    }
                    else
                    {
                        filtered_altitude = (filtered_altitude * 0.90f) + (raw_altitude * 0.10f);
                    }
                }

                localData.altitude = filtered_altitude;
                xQueueSend(Raw_Data_Queue, &localData, 0);
            }
            else
            {
                i2c_software_reset();
            }

            xSemaphoreGive(i2cMutex);
        }
    }
}

void FusionTask(void *argument)
{
    RawData_t localData;
    float dt;
    vTaskDelay(pdMS_TO_TICKS(100));
    while(1)
    {
        if(xQueueReceive(Raw_Data_Queue, &localData, portMAX_DELAY) == pdTRUE)
        {
            dt = calculate_dt(localData.timestamp);

            update_attitude(localData.ax, localData.ay, localData.az,
                            localData.gx, localData.gy,
                            localData.altitude, dt);

            xQueueSend(Display_Queue, &current_attitude, portMAX_DELAY);
        }
    }
}

void DisplayTask(void *argument)
{
    Attitude_t localData;

    char buf_pitch[16];
    char buf_roll[16];
    char buf_alt[16];

    int refresh_counter = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
    while(1)
    {
        if(xQueueReceive(Display_Queue, &localData, portMAX_DELAY) == pdTRUE)
        {
            refresh_counter++;

            if(refresh_counter >= 10)
            {
                refresh_counter = 0;

                sprintf(buf_pitch, "P: %5.1f", localData.pitch);
                
                sprintf(buf_roll,  "R: %5.1f", localData.roll);
                if(localData.altitude <= -999.0f)
                {
                    sprintf(buf_alt, "A: NO BARO");
                }
                else
                {
                    sprintf(buf_alt, "A: %4.1fM", localData.altitude);
                }

                xSemaphoreTake(i2cMutex, portMAX_DELAY);

                ssd1306_Fill(Black);

                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString(buf_pitch, Font_7x10, White);

                ssd1306_SetCursor(0, 15);
                ssd1306_WriteString(buf_roll, Font_7x10, White);

                ssd1306_SetCursor(0, 45);
                ssd1306_WriteString(buf_alt, Font_7x10, White);

                ssd1306_UpdateScreen();

                xSemaphoreGive(i2cMutex);
            }
        }
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while(1);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    while(1);
}
#endif