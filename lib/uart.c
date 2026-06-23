/*
 * Jupiter SDK — uart.c
 * Serial output via UART0. U-Boot leaves it configured at 115200 8N1.
 */
#include "jupiter.h"

void uart_putc(char c)
{
    while (!(UART0_LSR & UART0_LSR_THRE))
        ;
    UART0_THR = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_puthex(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putc(hex[(val >> i) & 0xF]);
}

void uart_putdec(uint32_t val)
{
    char buf[12];
    int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i--) uart_putc(buf[i]);
}
