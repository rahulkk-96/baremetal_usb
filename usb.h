#ifndef USB_H
#define USB_H

#include <stdint.h>
#include "uart.h"

// USB Core API
void usb_init(void);
void mock_virtual_host_reset(void);
void usb_poll(void);

#endif // USB_H