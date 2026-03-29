#include <stdint.h>
#include "uart.h"

/* =====================================================================
 * i.MX6UL UART1 Register Map and Bit Definitions
 * ===================================================================== */
#define UART1_BASE 0x02020000
#define UART1_UTXD (*(volatile uint32_t *)(UART1_BASE + 0x40))
#define UART1_UCR1 (*(volatile uint32_t *)(UART1_BASE + 0x80))
#define UART1_UCR2 (*(volatile uint32_t *)(UART1_BASE + 0x84))
#define UART1_USR2 (*(volatile uint32_t *)(UART1_BASE + 0x98))

#define UCR1_UARTEN (1 << 0)
#define UCR2_TXEN   (1 << 2)
#define UCR2_RXEN   (1 << 1)
#define USR2_TXDC   (1 << 3)

void uart_init(void) {
    UART1_UCR1 |= UCR1_UARTEN;
    UART1_UCR2 |= UCR2_TXEN | UCR2_RXEN;
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_putc('\r');
    }
    while (!(UART1_USR2 & USR2_TXDC));
    UART1_UTXD = c;
}

void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

void uart_print_hex(uint32_t val) {
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint32_t nibble = (val >> i) & 0xF;
        if (nibble < 10) {
            uart_putc('0' + nibble);
        } else {
            uart_putc('A' + (nibble - 10));
        }
    }
}