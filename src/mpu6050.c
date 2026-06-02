#include "mpu6050.h"
#include "i2c.h"
#include "stm32f4xx.h"

extern void uart2_printf(const char *fmt, ...);
extern void uart2_send_string(const char *s);

static void mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    i2c_write_reg(MPU6050_ADDR, reg, value);
}

static void mpu6050_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_read_bytes(MPU6050_ADDR, reg, buf, len);
}

int mpu6050_init(void)
{
    /* No delay here — main already waited 500ms */

    uint8_t who = 0xAA;
    mpu6050_read_regs(MPU6050_WHO_AM_I, &who, 1);
    uart2_printf("WHO_AM_I = 0x%02X\r\n", who);

    if (who != 0x68 && who != 0x70) return 1;

    /* Wake from sleep */
    mpu6050_write_reg(MPU6050_PWR_MGMT_1, 0x00);

    /* Wait for sensor to stabilise after wake */
    for (volatile int i = 0; i < 400000; i++);

    /* Discard first sample */
    uint8_t discard[6];
    mpu6050_read_regs(MPU6050_ACCEL_XOUT_H, discard, 6);

    return 0;
}

void mpu6050_read_accel(mpu6050_vec3_t *out)
{
    uint8_t buf[6];
    mpu6050_read_regs(MPU6050_ACCEL_XOUT_H, buf, 6);
    out->x = (int16_t)((buf[0] << 8) | buf[1]);
    out->y = (int16_t)((buf[2] << 8) | buf[3]);
    out->z = (int16_t)((buf[4] << 8) | buf[5]);
}

void mpu6050_read_gyro(mpu6050_vec3_t *out)
{
    uint8_t buf[6];
    mpu6050_read_regs(MPU6050_GYRO_XOUT_H, buf, 6);
    out->x = (int16_t)((buf[0] << 8) | buf[1]);
    out->y = (int16_t)((buf[2] << 8) | buf[3]);
    out->z = (int16_t)((buf[4] << 8) | buf[5]);
}