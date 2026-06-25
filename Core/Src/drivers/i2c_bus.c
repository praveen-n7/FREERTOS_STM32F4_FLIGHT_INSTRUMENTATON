#include "i2c_bus.h"
#include "stm32f4xx.h"


#define I2C_TIMEOUT 50000

// RETURN: 0 = Success, 1 = Error
uint8_t i2cWrite(uint8_t deviceAddr, uint8_t registerAddr, uint8_t *data, uint16_t length)
{
    volatile uint32_t timeout;

    // Check Bus Busy status
    timeout = I2C_TIMEOUT;
    while(I2C1->SR2 & (1<<1))
    {
        if(--timeout == 0) return 1;
    }

    // Send Start Bit
    I2C1->CR1 |= (1<<8);

    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<0))) // Wait for SB
    {
        if(--timeout == 0) return 1;
    }

    // Send Slave Address (write direction)
    I2C1->DR = deviceAddr << 1;

    timeout = I2C_TIMEOUT;
    while( !((I2C1->SR1 & (1<<1)) || (I2C1->SR1 & (1<<10))) )
    {
        if(--timeout == 0) return 1;
    }

    // NACK Check
    if(I2C1->SR1 & (1<<10))
    {
         I2C1->SR1 &= ~(1<<10);
         I2C1->CR1 |= (1<<9);
         return 1;
    }

    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; // Clear ADDR flag by reading SR1 then SR2

    // Send Register Address
    I2C1->DR = registerAddr;

    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<7))) // Wait for TXE
    {
        if(--timeout == 0) return 1;
    }

    // Send Data Payload
    for (uint16_t i = 0; i < length; i++)
    {
        I2C1->DR = data[i];

        timeout = I2C_TIMEOUT;
        while(!(I2C1->SR1 & (1<<7))) // Wait for TXE
        {
            if(--timeout == 0) return 1;
        }
    }

    // Wait for BTF (Byte Transfer Finished)
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<2)))
    {
        if(--timeout == 0) return 1;
    }

    // Send Stop Bit
    I2C1->CR1 |= (1<<9);

    return 0;
}

// RETURN: 0 = Success, 1 = Error (Timeout or NACK)
// Implements the three distinct read sequences from RM0090 Figures 164-166:
//   1-byte  : Set NACK+STOP before clearing ADDR, then read DR
//   2-byte  : Set POS+NACK before clearing ADDR, wait for BTF (both bytes in shift/DR),
//             then STOP, then read both bytes
//   N-byte  : Set ACK before clearing ADDR, NACK at byte N-2 using BTF, STOP before last read
uint8_t i2cRead(uint8_t deviceAddr, uint8_t registerAddr, uint8_t length, uint8_t *buffer)
{
    volatile uint32_t timeout;
    volatile uint32_t temp;

    if (length == 0) return 1;

    // ----------------------------------------------------------------
    // Phase 1: Write the register address (dummy write phase)
    // ----------------------------------------------------------------

    // Check Bus Busy
    timeout = I2C_TIMEOUT;
    while(I2C1->SR2 & (1<<1))
    {
        if(--timeout == 0) return 1;
    }

    // Send Start
    I2C1->CR1 |= (1<<8);

    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<0)))
    {
        if(--timeout == 0) return 1;
    }

    // Send address (write)
    I2C1->DR = deviceAddr << 1;

    timeout = I2C_TIMEOUT;
    while( !((I2C1->SR1 & (1<<1)) || (I2C1->SR1 & (1<<10))) )
    {
        if(--timeout == 0) return 1;
    }

    if(I2C1->SR1 & (1<<10)) // NACK
    {
        I2C1->SR1 &= ~(1<<10);
        I2C1->CR1 |= (1<<9);
        return 1;
    }

    temp = I2C1->SR1;
    temp = I2C1->SR2; // Clear ADDR

    // Send register address
    I2C1->DR = registerAddr;

    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<7))) // Wait TXE
    {
        if(--timeout == 0) return 1;
    }

    // ----------------------------------------------------------------
    // Phase 2: Repeated Start, then read N bytes
    // ----------------------------------------------------------------

    // Send Repeated Start
    I2C1->CR1 |= (1<<8);

    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & (1<<0)))
    {
        if(--timeout == 0) return 1;
    }

    // Send address (read direction)
    I2C1->DR = (deviceAddr << 1) | 1;

    timeout = I2C_TIMEOUT;
    while( !((I2C1->SR1 & (1<<1)) || (I2C1->SR1 & (1<<10))) )
    {
        if(--timeout == 0) return 1;
    }

    if(I2C1->SR1 & (1<<10)) // NACK
    {
        I2C1->SR1 &= ~(1<<10);
        I2C1->CR1 |= (1<<9);
        return 1;
    }

    // ----------------------------------------------------------------
    // Now apply the correct sequence BEFORE clearing ADDR
    // (RM0090 Figures 164, 165, 166)
    // ----------------------------------------------------------------

    if (length == 1)
    {
        // Figure 164: 1-byte read
        // Set NACK and STOP *before* clearing ADDR
        I2C1->CR1 &= ~(1<<10); // ACK=0 (NACK the only byte)
        temp = I2C1->SR1;
        temp = I2C1->SR2; // Clear ADDR — starts clocking the byte
        I2C1->CR1 |= (1<<9);  // STOP — must be set right after ADDR clear

        // Wait for RxNE
        timeout = I2C_TIMEOUT;
        while(!(I2C1->SR1 & (1<<6)))
        {
            if(--timeout == 0) return 1;
        }
        buffer[0] = I2C1->DR;
    }
    else if (length == 2)
    {
        // Figure 165: 2-byte read
        // Set POS and NACK *before* clearing ADDR
        // POS shifts the ACK/NACK control to act on the *next* byte (byte 2)
        I2C1->CR1 |= (1<<11);  // POS=1
        I2C1->CR1 &= ~(1<<10); // ACK=0
        temp = I2C1->SR1;
        temp = I2C1->SR2; // Clear ADDR

        // Wait for BTF — both bytes are now in shift register and DR
        timeout = I2C_TIMEOUT;
        while(!(I2C1->SR1 & (1<<2)))
        {
            if(--timeout == 0)
            {
                I2C1->CR1 &= ~(1<<11); // Clear POS before returning
                return 1;
            }
        }

        // STOP before reading to prevent a 3rd clock
        I2C1->CR1 |= (1<<9);

        buffer[0] = I2C1->DR; // Read byte 1
        buffer[1] = I2C1->DR; // Read byte 2

        // Clear POS for future reads
        I2C1->CR1 &= ~(1<<11);
    }
    else
    {
        // Figure 166: N-byte read (N >= 3)
        // ACK=1 before clearing ADDR so every byte gets ACK'd by default
        I2C1->CR1 |= (1<<10); // ACK=1
        temp = I2C1->SR1;
        temp = I2C1->SR2; // Clear ADDR

        for (int i = 0; i < length; i++)
        {
            if (i == length - 3)
            {
                // Wait for BTF: byte[N-3] in DR, byte[N-2] in shift register
                // This is the correct point to arm NACK so byte[N-1] gets NACK'd
                timeout = I2C_TIMEOUT;
                while(!(I2C1->SR1 & (1<<2))) // BTF
                {
                    if(--timeout == 0) return 1;
                }
                I2C1->CR1 &= ~(1<<10); // ACK=0 — next byte (N-1) will be NACK'd
            }
            else if (i == length - 2)
            {
                // Wait for BTF: byte[N-2] in DR, byte[N-1] fully received in shift register
                // Issue STOP now — it's safely queued after the current transfer
                timeout = I2C_TIMEOUT;
                while(!(I2C1->SR1 & (1<<2))) // BTF
                {
                    if(--timeout == 0) return 1;
                }
                I2C1->CR1 |= (1<<9); // STOP
            }
            else
            {
                // Middle bytes: wait for RxNE
                timeout = I2C_TIMEOUT;
                while(!(I2C1->SR1 & (1<<6))) // RxNE
                {
                    if(--timeout == 0) return 1;
                }
            }

            buffer[i] = I2C1->DR;
        }
    }

    return 0;
}


void i2c_software_reset(void)
{
    I2C1->CR1 |= (1<<15);   // SWRST=1
    I2C1->CR1 &= ~(1<<15);  // SWRST=0

    I2C1->CR1 &= ~(1<<0);   // PE=0 while reconfiguring

    // Restore timing — SWRST clears these to 0
    I2C1->CR2 = 42;
    I2C1->CCR = 210;
    I2C1->TRISE = 43;

    I2C1->CR1 |= (1<<10);   // ACK=1
    I2C1->CR1 |= (1<<0);    // PE=1
}