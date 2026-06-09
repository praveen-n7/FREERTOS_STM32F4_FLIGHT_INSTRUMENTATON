#ifndef UART_H
#define UART_H

#include "stm32f4xx.h"
#include <stdarg.h>          /* add this */

void uart2_init(uint32_t baud);
void uart2_send_char(char c);
void uart2_send_string(const char *s);
int  uart2_read_char(char *out);
void uart2_printf(const char *fmt, ...);   /* add this */

#endif