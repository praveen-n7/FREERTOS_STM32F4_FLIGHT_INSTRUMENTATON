#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <string.h>
#include "i2c.h"  /* your existing I2C1 driver from Project 2 */

#define SSD1306_I2C_ADDR     0x3C   /* SA0 pin low — default for most modules */
#define SSD1306_WIDTH        128
#define SSD1306_HEIGHT       64
#define SSD1306_PAGES        8      /* 64 pixels / 8 bits per page */

/* SSD1306 command bytes */
#define SSD1306_CTRL_CMD     0x00   /* Co=0, D/C#=0: all following bytes are commands */
#define SSD1306_CTRL_DATA    0x40   /* Co=0, D/C#=1: all following bytes are data */

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_set_cursor(uint8_t col, uint8_t row);  /* col 0-127, row 0-7 (page) */
void ssd1306_print_string(const char *str);
void ssd1306_update(void);  /* flush framebuffer to OLED over I2C */

#endif /* SSD1306_H */