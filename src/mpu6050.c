#include "mpu6050.h"
#include "i2c.h"

extern void uart2_send_string(const char *s);

/* -----------------------------------------------------------------------
 * Internal helpers — defined FIRST so all public functions can see them
 * -----------------------------------------------------------------------*/

static void mpu_write(uint8_t reg, uint8_t value)
{
    i2c_write_reg(MPU6050_ADDR, reg, value);
}

static void mpu_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_read_bytes(MPU6050_ADDR, reg, buf, len);
}

/* -----------------------------------------------------------------------
 * Public API — defined AFTER helpers
 * -----------------------------------------------------------------------*/

int mpu6050_init(void)
{
    uint8_t who = 0xAA;
    mpu_read(MPU6050_WHO_AM_I, &who, 1);

    {
        char msg[] = "MPU6050 WHO_AM_I = 0x00\r\n";
        uint8_t hi = (who >> 4) & 0x0F;
        uint8_t lo =  who       & 0x0F;
        msg[20] = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
        msg[21] = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
        uart2_send_string(msg);
    }

    if (who != MPU6050_WHO_AM_I_VALID_A && who != MPU6050_WHO_AM_I_VALID_B) {
        uart2_send_string("MPU6050 init FAILED\r\n");
        return 1;
    }

    mpu_write(MPU6050_PWR_MGMT_1, 0x00);

    for (volatile int i = 0; i < 400000; i++);

    uint8_t discard[6];
    mpu_read(MPU6050_ACCEL_XOUT_H, discard, 6);

    uart2_send_string("MPU6050 init OK\r\n");
    return 0;
}

void mpu6050_read_accel(mpu6050_vec3_t *out)
{
    uint8_t buf[6];
    mpu_read(MPU6050_ACCEL_XOUT_H, buf, 6);
    out->x = (int16_t)((buf[0] << 8) | buf[1]);
    out->y = (int16_t)((buf[2] << 8) | buf[3]);
    out->z = (int16_t)((buf[4] << 8) | buf[5]);
}

void mpu6050_read_gyro(mpu6050_vec3_t *out)
{
    uint8_t buf[6];
    mpu_read(MPU6050_GYRO_XOUT_H, buf, 6);
    out->x = (int16_t)((buf[0] << 8) | buf[1]);
    out->y = (int16_t)((buf[2] << 8) | buf[3]);
    out->z = (int16_t)((buf[4] << 8) | buf[5]);
}

void mpu6050_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[14];
    mpu_read(MPU6050_ACCEL_XOUT_H, buf, 14);

    *ax = (int16_t)((buf[0]  << 8) | buf[1]);
    *ay = (int16_t)((buf[2]  << 8) | buf[3]);
    *az = (int16_t)((buf[4]  << 8) | buf[5]);

    /* buf[6] and buf[7] = TEMP — skipped */

    *gx = (int16_t)((buf[8]  << 8) | buf[9]);
    *gy = (int16_t)((buf[10] << 8) | buf[11]);
    *gz = (int16_t)((buf[12] << 8) | buf[13]);
}