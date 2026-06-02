#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void    i2c1_init(void);
void    i2c_read_bytes(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);
void    i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value);

/*
 * Generic I2C write — sends len bytes from buf to addr7.
 * Used by SSD1306 driver: buf[0] is the control byte (0x00 or 0x40),
 * followed by command/data bytes. No register-address phase.
 */
void    i2c1_write(uint8_t addr7, const uint8_t *buf, uint8_t len);

/* Legacy stubs — kept for compatibility */
void    i2c_start(void);
void    i2c_stop(void);
uint8_t i2c_send_addr(uint8_t addr7, uint8_t dir);
uint8_t i2c_write_byte(uint8_t data);
uint8_t i2c_read_ack(void);
uint8_t i2c_read_nack(void);

#endif