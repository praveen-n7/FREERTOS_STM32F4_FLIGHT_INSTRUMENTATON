#include "i2c.h"
#include "stm32f4xx.h"

static uint8_t wait_sr1(uint32_t mask, uint32_t timeout)
{
    while (!(I2C1->SR1 & mask)) {
        if (--timeout == 0) return 0;
    }
    return 1;
}

static void bus_wait(void)
{
    uint32_t t = 100000;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --t);
}

void i2c1_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    volatile uint32_t d = RCC->APB1ENR; (void)d;

    GPIOB->MODER   &= ~((3U<<12)|(3U<<14));
    GPIOB->MODER   |=  ((2U<<12)|(2U<<14));
    GPIOB->OTYPER  |=  (1U<<6)|(1U<<7);
    GPIOB->PUPDR   &= ~((3U<<12)|(3U<<14));
    GPIOB->OSPEEDR |=  ((3U<<12)|(3U<<14));
    GPIOB->AFR[0]  &= ~((0xFU<<24)|(0xFU<<28));
    GPIOB->AFR[0]  |=  ((4U<<24)|(4U<<28));

    I2C1->CR1 |=  I2C_CR1_SWRST;
    for (volatile int i = 0; i < 2000; i++);
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    for (volatile int i = 0; i < 2000; i++);

    I2C1->CR1   = 0;
    I2C1->CR2   = 16;
    I2C1->CCR   = 80;
    I2C1->TRISE = 17;
    I2C1->CR1   = I2C_CR1_PE;
}

void i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    bus_wait();

    I2C1->CR1 &= ~(I2C_CR1_POS | I2C_CR1_ACK);

    /* START */
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB, 100000)) goto stop;
    (void)I2C1->SR1;
    I2C1->DR = (uint8_t)((addr7 << 1) | 0);

    /* ADDR */
    if (!wait_sr1(I2C_SR1_ADDR | I2C_SR1_AF, 100000)) goto stop;
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto stop; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    /* reg */
    if (!wait_sr1(I2C_SR1_TXE, 100000)) goto stop;
    I2C1->DR = reg;

    /* value */
    if (!wait_sr1(I2C_SR1_TXE, 100000)) goto stop;
    I2C1->DR = value;

    if (!wait_sr1(I2C_SR1_BTF, 100000)) goto stop;

stop:
    I2C1->CR1 |= I2C_CR1_STOP;
    bus_wait();
}

/*
 * i2c1_write — generic multi-byte write used by SSD1306.
 *
 * Sends: START → addr+W → buf[0] → buf[1] → ... → buf[len-1] → STOP
 *
 * For SSD1306:
 *   Command:  buf = {0x00, cmd_byte}         len=2
 *   Data:     buf = {0x40, d0, d1, ... d127} len=129
 *
 * No register-address phase — the control byte is already buf[0].
 */
void i2c1_write(uint8_t addr7, const uint8_t *buf, uint8_t len)
{
    if (len == 0) return;

    bus_wait();
    I2C1->CR1 &= ~(I2C_CR1_POS | I2C_CR1_ACK);

    /* START */
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB, 100000)) goto stop;
    (void)I2C1->SR1;
    I2C1->DR = (uint8_t)((addr7 << 1) | 0);   /* write direction */

    /* ADDR cleared */
    if (!wait_sr1(I2C_SR1_ADDR | I2C_SR1_AF, 100000)) goto stop;
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto stop; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    /* Send all bytes */
    for (uint8_t i = 0; i < len; i++) {
        if (!wait_sr1(I2C_SR1_TXE, 100000)) goto stop;
        I2C1->DR = buf[i];
    }

    /* Wait for last byte to shift out */
    if (!wait_sr1(I2C_SR1_BTF, 100000)) goto stop;

stop:
    I2C1->CR1 |= I2C_CR1_STOP;
    bus_wait();
}

void i2c_read_bytes(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) return;
    uint32_t t;

    bus_wait();
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |=  I2C_CR1_ACK;

    /* Phase 1: write register address */
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB, 100000)) goto fail;
    (void)I2C1->SR1;
    I2C1->DR = (uint8_t)((addr7 << 1) | 0);

    if (!wait_sr1(I2C_SR1_ADDR | I2C_SR1_AF, 100000)) goto fail;
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto fail; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    if (!wait_sr1(I2C_SR1_TXE, 100000)) goto fail;
    I2C1->DR = reg;
    if (!wait_sr1(I2C_SR1_BTF, 100000)) goto fail;

    /* Phase 2: repeated START + read */
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB, 100000)) goto fail;
    (void)I2C1->SR1;
    I2C1->DR = (uint8_t)((addr7 << 1) | 1);

    if (!wait_sr1(I2C_SR1_ADDR | I2C_SR1_AF, 100000)) goto fail;
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto fail; }

    I2C1->CR1 |=  I2C_CR1_POS;
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR1; (void)I2C1->SR2;   /* clear ADDR */

    if (len == 1) {
        if (!wait_sr1(I2C_SR1_BTF, 100000)) goto fail;
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[0] = (uint8_t)I2C1->DR;
        (void)I2C1->DR;

    } else if (len == 2) {
        if (!wait_sr1(I2C_SR1_BTF, 100000)) goto fail;
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[0] = (uint8_t)I2C1->DR;
        buf[1] = (uint8_t)I2C1->DR;

    } else {
        I2C1->CR1 &= ~I2C_CR1_POS;
        I2C1->CR1 |=  I2C_CR1_ACK;

        for (uint8_t i = 0; i < (uint8_t)(len - 3); i++) {
            if (!wait_sr1(I2C_SR1_RXNE, 100000)) goto fail;
            buf[i] = (uint8_t)I2C1->DR;
        }

        if (!wait_sr1(I2C_SR1_BTF, 100000)) goto fail;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        buf[len-3] = (uint8_t)I2C1->DR;
        I2C1->CR1 |=  I2C_CR1_STOP;
        buf[len-2] = (uint8_t)I2C1->DR;
        if (!wait_sr1(I2C_SR1_RXNE, 100000)) goto fail;
        buf[len-1] = (uint8_t)I2C1->DR;
    }

    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |=  I2C_CR1_ACK;
    bus_wait();
    return;

fail:
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |=  I2C_CR1_ACK;
    I2C1->CR1 |=  I2C_CR1_STOP;
    t = 100000; while ((I2C1->SR2 & I2C_SR2_BUSY) && --t);
}

/* Legacy stubs */
void    i2c_start(void)                      {}
void    i2c_stop(void)                       {}
uint8_t i2c_write_byte(uint8_t d)           { (void)d; return 0; }
uint8_t i2c_read_ack(void)                   { return 0; }
uint8_t i2c_read_nack(void)                  { return 0; }
uint8_t i2c_send_addr(uint8_t a, uint8_t d) { (void)a; (void)d; return 0; }