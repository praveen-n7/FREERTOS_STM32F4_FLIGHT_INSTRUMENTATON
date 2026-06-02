#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/* I2C address — AD0 pin low = 0x68, high = 0x69 */
#define MPU6050_ADDR              0x68

/* Register addresses */
#define MPU6050_PWR_MGMT_1        0x6B
#define MPU6050_WHO_AM_I          0x75
#define MPU6050_ACCEL_XOUT_H      0x3B
#define MPU6050_GYRO_XOUT_H       0x43

#define MPU6050_WHO_AM_I_VALID_A  0x68   /* genuine MPU6050 */
#define MPU6050_WHO_AM_I_VALID_B  0x70   /* clone — ICM-20600 or similar */

typedef struct {
    int16_t x, y, z;
} mpu6050_vec3_t;

/*
 * Returns 0 on success, 1 on failure (WHO_AM_I mismatch or bus error).
 * Call once at startup before FreeRTOS scheduler starts.
 */
int  mpu6050_init(void);

/* Read accelerometer only */
void mpu6050_read_accel(mpu6050_vec3_t *out);

/* Read gyroscope only */
void mpu6050_read_gyro(mpu6050_vec3_t *out);

/*
 * mpu6050_read_raw — reads accel + gyro in ONE I2C transaction (14 bytes).
 *
 * This is what sensor_task.c calls every 50ms.
 * Reading both in one burst is faster and keeps accel+gyro timestamps matched.
 *
 * All six values are raw ADC counts (±32768 at full scale).
 * At default ±2g accel range:  1g = 16384 counts
 * At default ±250°/s gyro range: 1°/s = 131 counts
 */
void mpu6050_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz);

#endif