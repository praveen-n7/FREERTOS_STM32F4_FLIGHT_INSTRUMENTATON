#include "i2c_bus.h"
#include "mpu6050.h"
#include "delay.h"


static int16_t gyro_x_offset = 0;
static int16_t gyro_y_offset = 0;
static int16_t gyro_z_offset = 0;

void MPU_WriteReg(uint8_t reg, uint8_t value, uint8_t length) {
    i2cWrite(MPU6050_ADDR, reg, &value, length);
}

uint8_t mpu6050_whoami(void)
{
    uint8_t whoAmIBuffer;
    i2cRead(MPU6050_ADDR, WHO_AM_I, 1, &whoAmIBuffer);
    return whoAmIBuffer;
}

void mpu6050_init(void)
{
    MPU_WriteReg(PWR_MGMT_1, 0x03, 1); // Clock: PLL with Z gyro
    MPU_WriteReg(CONFIGM,    0x03, 1); // DLPF 44Hz
    MPU_WriteReg(ACCEL_CONFIG, 0x00, 1); // +/- 2g
    MPU_WriteReg(GYRO_CONFIG,  0x00, 1); // +/- 250 deg/s
    // Do NOT enable INT_ENABLE — we poll directly without needing the INT pin
}

void mpu6050_calibrate_gyro(void)
{
    int32_t x_sum = 0, y_sum = 0, z_sum = 0;
    int16_t gx, gy, gz;

    for (int i = 0; i < 200; i++)
    {
        uint8_t buf[6];
        i2cRead(MPU6050_ADDR, GYRO_XOUT_H, 6, buf);

        gx = (int16_t)((buf[0] << 8) | buf[1]);
        gy = (int16_t)((buf[2] << 8) | buf[3]);
        gz = (int16_t)((buf[4] << 8) | buf[5]);

        x_sum += gx;
        y_sum += gy;
        z_sum += gz;

        delay_ms(2);
    }

    gyro_x_offset = x_sum / 200;
    gyro_y_offset = y_sum / 200;
    gyro_z_offset = z_sum / 200;
}

// Returns 0 on success, 1 on I2C error
uint8_t mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];
    uint8_t status = i2cRead(MPU6050_ADDR, ACCEL_XOUT_H, 6, buf);
    if (status != 0) return 1;

    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);
    return 0;
}

// Returns 0 on success, 1 on I2C error
uint8_t mpu6050_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[6];
    uint8_t status = i2cRead(MPU6050_ADDR, GYRO_XOUT_H, 6, buf);
    if (status != 0) return 1;

    *gx = (int16_t)((buf[0] << 8) | buf[1]) - gyro_x_offset;
    *gy = (int16_t)((buf[2] << 8) | buf[3]) - gyro_y_offset;
    *gz = (int16_t)((buf[4] << 8) | buf[5]) - gyro_z_offset;
    return 0;
}