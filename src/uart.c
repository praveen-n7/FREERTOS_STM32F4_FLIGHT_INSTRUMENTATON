#include "uart.h"
#include <stdio.h>      /* add this at the top with other includes */
#include <stdarg.h>     /* add this at the top */


/* ── Ring buffer for RX ── */
#define RX_BUF_SIZE 64
static volatile char    rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

/* ── Init ── */
void uart2_init(uint32_t baud) {

    /* 1. Enable clocks */
    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->APB1ENR |= (1 << 17);  // USART2 clock

    /* 2. PA2=TX, PA3=RX → Alternate Function mode (MODER = 10) */
    GPIOA->MODER &= ~(0xF << 4);
    GPIOA->MODER |=  (0xA << 4);

    /* 3. Assign AF7 (USART2) to PA2 and PA3 */
    GPIOA->AFR[0] &= ~(0xFF << 8);
    GPIOA->AFR[0] |=  (0x77 << 8);

   
    /* was 8000000, now 16000000 */
    USART2->BRR = (16000000 + baud / 2) / baud;

    /* 5. Enable TX, RX, USART, and RXNE interrupt */
    USART2->CR1 = (1 << 3)   // TE  — transmitter enable
                | (1 << 2)   // RE  — receiver enable
                | (1 << 5)   // RXNEIE — interrupt on byte received
                | (1 << 13); // UE  — USART enable

    /* 6. Enable USART2 interrupt in NVIC */
    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* ── Transmit ── */
void uart2_send_char(char c) {
    while (!(USART2->SR & (1 << 7)));  // wait for TXE (TX register empty)
    USART2->DR = (uint8_t)c;
}

void uart2_send_string(const char *s) {
    while (*s) uart2_send_char(*s++);
}

/* ── Receive ISR — fires automatically on each incoming byte ── */
void USART2_IRQHandler(void) {
    if (USART2->SR & (1 << 5)) {            // RXNE set?
        char c = (char)(USART2->DR & 0xFF); // reading DR clears RXNE
        uint8_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) {              // buffer not full
            rx_buf[rx_head] = c;
            rx_head = next;
        }
    }
}

/* ── Read from ring buffer (call from main loop) ── */
int uart2_read_char(char *out) {
    if (rx_tail == rx_head) return 0;       // empty
    *out    = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return 1;
}
/* ── Printf wrapper — add this at the very bottom ── */
void uart2_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart2_send_string(buf);   /* reuses your existing transmit function */
}