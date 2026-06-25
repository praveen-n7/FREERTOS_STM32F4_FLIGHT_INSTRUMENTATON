#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#define MPU6050_ADDR     0x68
#define WHO_AM_I         0x75  ///who ami changed
#define PWR_MGMT_1       0x6B
#define CONFIGM          0x1A
#define GYRO_CONFIG      0x1B
#define ACCEL_CONFIG     0x1C
#define INT_ENABLE       0x38
#define INT_STATUS     	 0X3A

#define ACCEL_XOUT_H     0x3B
#define GYRO_XOUT_H      0x43

void mpu6050_init(void);
void mpu6050_calibrate_gyro(void);
uint8_t mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az);
uint8_t mpu6050_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz);
uint8_t mpu6050_whoami(void);

#endif // MPU6050_H
