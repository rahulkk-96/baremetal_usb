#include <stdint.h>
#include "usb.h"
#include "uart.h"

void main(void) {
    uart_init();

    uart_puts("========================================\n");
    uart_puts(" Bare-metal USB Enumeration Project\n");
    uart_puts(" Target: i.MX6UL (Synopsys Atlantic)\n");
    uart_puts("========================================\n");
    uart_puts("[INFO] Boot successful. Stack and BSS initialized.\n");

    usb_init();

    mock_virtual_host_reset();
    uart_puts("[INFO] Waiting for USB events...\n");

    while (1) {
        usb_poll();
    }
}
