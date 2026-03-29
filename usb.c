#include <stdint.h>
#include <stddef.h>
#include "usb.h"

/* =====================================================================
 * USB 2.0 Synopsys Atlantic (Chipidea) Data Structures
 * ===================================================================== */

// Standard USB 2.0 Setup Packet (8 bytes)
typedef struct __attribute__((packed, aligned(4))) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

// Standard Request Codes
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09

/* =====================================================================
 * USB Descriptors
 * ===================================================================== */
// Standard USB 2.0 Device Descriptor (18 bytes)
__attribute__((aligned(32))) const uint8_t device_descriptor[18] = {
    18,         // bLength: 18 bytes
    0x01,       // bDescriptorType: DEVICE
    0x00, 0x02, // bcdUSB: USB 2.0 (0x0200, Little Endian)
    0x00,       // bDeviceClass: (Defined at interface level)
    0x00,       // bDeviceSubClass: 0
    0x00,       // bDeviceProtocol: 0
    64,         // bMaxPacketSize0: 64 bytes (Matches ep0_caps!)
    0x34, 0x12, // idVendor: Mock Vendor ID (0x1234, Little Endian)
    0xCD, 0xAB, // idProduct: Mock Product ID (0xABCD, Little Endian)
    0x00, 0x01, // bcdDevice: v1.00 (0x0100, Little Endian)
    0x01,       // iManufacturer: 1
    0x02,       // iProduct: 2
    0x03,       // iSerialNumber: 3
    0x01        // bNumConfigurations: 1
};

// Standard USB 2.0 Configuration Descriptor Block (32 bytes total)
__attribute__((aligned(32))) const uint8_t config_descriptor[32] = {
    // Configuration Descriptor (9 bytes)
    0x09,       // bLength
    0x02,       // bDescriptorType: CONFIGURATION
    0x20, 0x00, // wTotalLength: 32 bytes
    0x01,       // bNumInterfaces: 1
    0x01,       // bConfigurationValue: 1
    0x00,       // iConfiguration: 0
    0xC0,       // bmAttributes: Self-powered
    0x32,       // bMaxPower: 100mA (50 * 2)

    // Interface Descriptor (9 bytes)
    0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0x00, 
    /* bLength, bDescriptorType(INTERFACE), bInterfaceNumber, bAlternateSetting, 
       bNumEndpoints(2), bInterfaceClass(Vendor), bInterfaceSubClass, bInterfaceProtocol, iInterface */

    // Endpoint Descriptor (EP1 IN) (7 bytes)
    0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,
    /* bLength, bDescriptorType(ENDPOINT), bEndpointAddress(EP1 IN), bmAttributes(Bulk), wMaxPacketSize(512), bInterval */

    // Endpoint Descriptor (EP1 OUT) (7 bytes)
    0x07, 0x05, 0x01, 0x02, 0x00, 0x02, 0x00
};

// String Descriptor 0 (Language ID: English US = 0x0409)
__attribute__((aligned(32))) const uint8_t string_desc_0[4] = {
    0x04, 0x03, 0x09, 0x04
};

// String Descriptor 1 (Manufacturer: "Acme")
__attribute__((aligned(32))) const uint8_t string_desc_1[10] = {
    0x0A, 0x03, 'A', 0, 'c', 0, 'm', 0, 'e', 0
};

// String Descriptor 2 (Product: "My USB")
__attribute__((aligned(32))) const uint8_t string_desc_2[14] = {
    0x0E, 0x03, 'M', 0, 'y', 0, ' ', 0, 'U', 0, 'S', 0, 'B', 0
};

// String Descriptor 3 (Serial: "123")
__attribute__((aligned(32))) const uint8_t string_desc_3[8] = {
    0x08, 0x03, '1', 0, '2', 0, '3', 0
};

const uint8_t* const string_descriptors[] = { string_desc_0, string_desc_1, string_desc_2, string_desc_3 };
const uint8_t string_descriptor_lengths[] = { sizeof(string_desc_0), sizeof(string_desc_1), sizeof(string_desc_2), sizeof(string_desc_3) };

// Device Transfer Descriptor (dTD) - 32 Bytes
typedef struct __attribute__((aligned(32))) {
    volatile uint32_t next_dtd_ptr;
    volatile uint32_t token;
    volatile uint32_t page_ptr[5];
    volatile uint32_t reserved;
} dTD_t;

// Device Queue Head (dQH) - 64 Bytes
typedef struct __attribute__((aligned(64))) {
    volatile uint32_t cap;
    volatile uint32_t curr_dtd_ptr;
    volatile uint32_t next_dtd_ptr;
    volatile uint32_t token;
    volatile uint32_t page_ptr[5];
    volatile uint32_t reserved;
    volatile uint32_t setup_buf[2]; // 8-byte SETUP packet written here by hardware
    volatile uint32_t reserved2[4];
} dQH_t;

#define NUM_EPS 2
#define EP_OUT_IDX(ep) ((ep) * 2)
#define EP_IN_IDX(ep)  ((ep) * 2 + 1)

// Dynamically allocated endpoint arrays
volatile dQH_t *endpoint_list;
volatile dTD_t *ep_in_dtd[NUM_EPS];
volatile dTD_t *ep_out_dtd[NUM_EPS];
volatile uint8_t *ep_rx_buffer[NUM_EPS];
volatile uint8_t *ep_tx_buffer[NUM_EPS];

// Uncached Bump Allocator
extern uint32_t __uncached_start;
extern uint32_t __uncached_end;
static uintptr_t uncached_heap_curr = 0;

void* uncached_memalign(size_t alignment, size_t size) {
    if (uncached_heap_curr == 0) {
        uncached_heap_curr = (uintptr_t)&__uncached_start;
    }
    uintptr_t ptr = (uncached_heap_curr + alignment - 1) & ~(alignment - 1);
    if (ptr + size > (uintptr_t)&__uncached_end) {
        uart_puts("[ERROR] Out of uncached memory!\n");
        while(1);
    }
    uncached_heap_curr = ptr + size;
    
    // Zero out allocated memory safely
    uint8_t *p = (uint8_t*)ptr;
    for(size_t i = 0; i < size; i++) p[i] = 0;
    
    return (void*)ptr;
}

// A standard malloc-like API that defaults to 32-byte alignment for USB structures
void* usb_malloc(size_t size) {
    return uncached_memalign(32, size);
}

/* =====================================================================
 * i.MX6UL USB1 (OTG1) Register Map and Bit Definitions
 * ===================================================================== */
#define USB1_BASE 0x02184000

#define USB_USBCMD           (*(volatile uint32_t *)(USB1_BASE + 0x140))
#define USB_USBSTS           (*(volatile uint32_t *)(USB1_BASE + 0x144))
#define USB_DEVICEADDR       (*(volatile uint32_t *)(USB1_BASE + 0x154))
#define USB_ENDPOINTLISTADDR (*(volatile uint32_t *)(USB1_BASE + 0x158))
#define USB_ENDPTSETUPSTAT   (*(volatile uint32_t *)(USB1_BASE + 0x1AC))
#define USB_ENDPTFLUSH       (*(volatile uint32_t *)(USB1_BASE + 0x1B4))
#define USB_ENDPTCOMPLETE    (*(volatile uint32_t *)(USB1_BASE + 0x1BC))
#define USB_ENDPTPRIME       (*(volatile uint32_t *)(USB1_BASE + 0x1B0))
#define USB_ENDPTCTRL0       (*(volatile uint32_t *)(USB1_BASE + 0x1C0))
#define USB_ENDPTCTRL1       (*(volatile uint32_t *)(USB1_BASE + 0x1C4))
#define USB_USBMODE          (*(volatile uint32_t *)(USB1_BASE + 0x1A8))

#define USBCMD_RS            (1 << 0)  // Run/Stop
#define USBCMD_RST           (1 << 1)  // Controller Reset
#define USBCMD_SUTW          (1 << 13) // Setup Tripwire

#define USBSTS_UI            (1 << 0)  // USB Interrupt (Transfer/Setup)
#define USBSTS_URI           (1 << 6)  // USB Reset Received

#define USBMODE_CM_DEVICE    (2 << 0)  // Controller Mode: Device
#define USBMODE_SLOM         (1 << 3)  // Setup Lockout Mode

/* =====================================================================
 * Mock Hardware Injection
 * ===================================================================== */
volatile uint32_t mock_hardware_usbsts = 0;
volatile uint32_t mock_hardware_endptsetupstat = 0;
volatile uint32_t mock_hardware_endptcomplete = 0;

typedef enum {
    STATE_DEFAULT,
    STATE_ADDRESS_PENDING,
    STATE_ADDRESSED,
    STATE_CONFIG_FETCHED,
    STATE_CONFIGURED
} device_state_t;

volatile device_state_t device_state = STATE_DEFAULT;
volatile uint8_t pending_address = 0;

void usb_init(void) {
    // Allocate the endpoint list (2 entries per endpoint for OUT and IN)
    endpoint_list = (volatile dQH_t *)uncached_memalign(2048, (NUM_EPS * 2) * sizeof(dQH_t));

    // Allocate the dTDs and buffers for all endpoints dynamically
    for (int i = 0; i < NUM_EPS; i++) {
        ep_in_dtd[i]    = (volatile dTD_t *)usb_malloc(sizeof(dTD_t));
        ep_out_dtd[i]   = (volatile dTD_t *)usb_malloc(sizeof(dTD_t));
        ep_rx_buffer[i] = (volatile uint8_t *)usb_malloc(512);
        ep_tx_buffer[i] = (volatile uint8_t *)usb_malloc(512);
    }

    uart_puts("[INFO] endpoint_list address: ");
    uart_print_hex((uint32_t)endpoint_list);
    uart_puts("\n");

    uart_puts("[INFO] Initializing USB Controller...\n");

    USB_USBCMD &= ~USBCMD_RS;
    USB_USBCMD |= USBCMD_RST;
    while (USB_USBCMD & USBCMD_RST) {}

    USB_USBMODE = USBMODE_CM_DEVICE | USBMODE_SLOM;

    uint32_t ep0_caps = (64 << 16) | (1 << 15);
    endpoint_list[EP_OUT_IDX(0)].cap = ep0_caps;
    endpoint_list[EP_OUT_IDX(0)].next_dtd_ptr = 1; 
    endpoint_list[EP_IN_IDX(0)].cap = ep0_caps;
    endpoint_list[EP_IN_IDX(0)].next_dtd_ptr = 1;

    USB_ENDPOINTLISTADDR = (uint32_t)endpoint_list;
    USB_USBCMD |= USBCMD_RS;
    uart_puts("[INFO] USB Controller Started in Device Mode.\n");
}

void send_device_descriptor(void) {
    uart_puts("[INFO] Preparing dTD to send Device Descriptor...\n");
    ep_in_dtd[0]->next_dtd_ptr = 0x01;
    ep_in_dtd[0]->page_ptr[0]  = (uint32_t)&device_descriptor;
    ep_in_dtd[0]->token = (18 << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_IN_IDX(0)].next_dtd_ptr = (uint32_t)ep_in_dtd[0] & ~0x01;
    endpoint_list[EP_IN_IDX(0)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 16);
}

void send_config_descriptor(uint16_t length) {
    uart_puts("[INFO] Preparing dTD to send Configuration Descriptor...\n");
    uint32_t transfer_len = (length < sizeof(config_descriptor)) ? length : sizeof(config_descriptor);
    ep_in_dtd[0]->next_dtd_ptr = 0x01;
    ep_in_dtd[0]->page_ptr[0]  = (uint32_t)&config_descriptor;
    ep_in_dtd[0]->token = (transfer_len << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_IN_IDX(0)].next_dtd_ptr = (uint32_t)ep_in_dtd[0] & ~0x01;
    endpoint_list[EP_IN_IDX(0)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 16);
}

void stall_ep0(void) {
    uart_puts("[INFO] Stalling EP0 to reject unsupported request...\n");
    USB_ENDPTCTRL0 |= (1 << 16) | (1 << 0);
}

void send_string_descriptor(uint8_t index, uint16_t length) {
    uart_puts("[INFO] Preparing dTD to send String Descriptor ");
    uart_putc('0' + index);
    uart_puts("...\n");
    if (index >= (sizeof(string_descriptors)/sizeof(string_descriptors[0]))) {
        stall_ep0();
        return;
    }
    uint32_t transfer_len = (length < string_descriptor_lengths[index]) ? length : string_descriptor_lengths[index];
    ep_in_dtd[0]->next_dtd_ptr = 0x01;
    ep_in_dtd[0]->page_ptr[0]  = (uint32_t)string_descriptors[index];
    ep_in_dtd[0]->token = (transfer_len << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_IN_IDX(0)].next_dtd_ptr = (uint32_t)ep_in_dtd[0] & ~0x01;
    endpoint_list[EP_IN_IDX(0)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 16);
}

void send_ep0_status_phase(void) {
    uart_puts("[INFO] Sending Zero-Length Packet for Status Phase...\n");
    ep_in_dtd[0]->next_dtd_ptr = 0x01;
    ep_in_dtd[0]->page_ptr[0]  = 0;
    ep_in_dtd[0]->token = (0 << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_IN_IDX(0)].next_dtd_ptr = (uint32_t)ep_in_dtd[0] & ~0x01;
    endpoint_list[EP_IN_IDX(0)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 16);
}

void prime_ep1_out(void) {
    uart_puts("[INFO] Priming EP1 OUT to receive bulk data...\n");
    ep_out_dtd[1]->next_dtd_ptr = 0x01;
    ep_out_dtd[1]->page_ptr[0]  = (uint32_t)ep_rx_buffer[1];
    ep_out_dtd[1]->token = (512 << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_OUT_IDX(1)].next_dtd_ptr = (uint32_t)ep_out_dtd[1] & ~0x01;
    endpoint_list[EP_OUT_IDX(1)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 1);
}

void prime_ep1_in(uint32_t length) {
    uart_puts("[INFO] Priming EP1 IN to send bulk data...\n");
    ep_in_dtd[1]->next_dtd_ptr = 0x01;
    ep_in_dtd[1]->page_ptr[0]  = (uint32_t)ep_tx_buffer[1];
    ep_in_dtd[1]->token = (length << 16) | (1 << 15) | (1 << 7);
    endpoint_list[EP_IN_IDX(1)].next_dtd_ptr = (uint32_t)ep_in_dtd[1] & ~0x01;
    endpoint_list[EP_IN_IDX(1)].token &= ~(1 << 7);
    USB_ENDPTPRIME = (1 << 17);
}

void mock_virtual_host_fetch_data(void);
void mock_virtual_host_set_configuration(void);
void mock_virtual_host_get_config_descriptor(void);
void mock_virtual_host_get_string_descriptor(void);
void mock_virtual_host_send_ep1_bulk(void);
void mock_virtual_host_get_status(void);
void mock_virtual_host_fetch_ep1_bulk(void);

void handle_ep0_setup(void) {
    usb_setup_packet_t setup_req;
    uint32_t *setup_dest = (uint32_t *)&setup_req;
    uart_puts("[INFO] EP0 SETUP Packet Received. Reading via Tripwire...\n");
    do {
        USB_USBCMD |= USBCMD_SUTW;
        setup_dest[0] = endpoint_list[EP_OUT_IDX(0)].setup_buf[0];
        setup_dest[1] = endpoint_list[EP_OUT_IDX(0)].setup_buf[1];
    } while (!(USB_USBCMD & USBCMD_SUTW)); 
    USB_USBCMD &= ~USBCMD_SUTW;
    USB_ENDPTSETUPSTAT = (1 << 0);
    mock_hardware_endptsetupstat &= ~(1 << 0);
    USB_ENDPTFLUSH = (1 << 16) | (1 << 0);
    while (USB_ENDPTFLUSH);
    uart_puts("[INFO] SETUP Packet Parsed:\n");
    uart_puts("       bmRequestType: "); uart_print_hex(setup_req.bmRequestType); uart_puts("\n");
    uart_puts("       bRequest:      "); uart_print_hex(setup_req.bRequest); uart_puts("\n");
    uart_puts("       wValue:        "); uart_print_hex(setup_req.wValue); uart_puts("\n");
    if (setup_req.bmRequestType == 0x80 && setup_req.bRequest == USB_REQ_GET_DESCRIPTOR) {
        uint8_t desc_type = setup_req.wValue >> 8;
        if (desc_type == 0x01) {
            uart_puts("\n[ACTION] Host requested GET_DESCRIPTOR (Device)!\n");
            send_device_descriptor();
            mock_virtual_host_fetch_data();
        } else if (desc_type == 0x02) {
            uart_puts("\n[ACTION] Host requested GET_DESCRIPTOR (Configuration)!\n");
            send_config_descriptor(setup_req.wLength);
            mock_virtual_host_fetch_data();
        } else if (desc_type == 0x03) {
            uart_puts("\n[ACTION] Host requested GET_DESCRIPTOR (String)!\n");
            send_string_descriptor(setup_req.wValue & 0xFF, setup_req.wLength);
            mock_virtual_host_fetch_data();
        }
    } else if (setup_req.bmRequestType == 0x00 && setup_req.bRequest == USB_REQ_SET_ADDRESS) {
        uart_puts("\n[ACTION] Host requested SET_ADDRESS!\n");
        pending_address = setup_req.wValue & 0x7F;
        device_state = STATE_ADDRESS_PENDING;
        send_ep0_status_phase();
        mock_virtual_host_fetch_data();
    } else if (setup_req.bmRequestType == 0x00 && setup_req.bRequest == USB_REQ_SET_CONFIGURATION) {
        uart_puts("\n[ACTION] Host requested SET_CONFIGURATION!\n");
        USB_ENDPTCTRL1 = (1 << 23) | (1 << 22) | (2 << 18) | (1 << 7) | (1 << 6) | (2 << 2);
        uint32_t ep1_caps = (512 << 16) | (1 << 15);
        endpoint_list[EP_OUT_IDX(1)].cap = ep1_caps;
        endpoint_list[EP_OUT_IDX(1)].next_dtd_ptr = 1;
        endpoint_list[EP_IN_IDX(1)].cap = ep1_caps;
        endpoint_list[EP_IN_IDX(1)].next_dtd_ptr = 1;
        device_state = STATE_CONFIGURED;
        send_ep0_status_phase();
        mock_virtual_host_fetch_data();
    } else {
        uart_puts("\n[ACTION] Unsupported SETUP request received. Issuing STALL!\n");
        stall_ep0();
    }
}

void mock_virtual_host_fetch_data(void) {
    uart_puts("\n[MOCK HOST] Fetching primed data from EP0 IN...\n");
    USB_ENDPTPRIME &= ~(1 << 16);
    mock_hardware_endptcomplete |= (1 << 16);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_set_address(void) {
    uart_puts("\n[MOCK HOST] Sending SET_ADDRESS Setup Packet on EP0...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x00070500;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00000000;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_get_config_descriptor(void) {
    uart_puts("\n[MOCK HOST] Sending GET_DESCRIPTOR (Configuration) Setup Packet on EP0...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x02000680;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00FF0000;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_get_string_descriptor(void) {
    uart_puts("\n[MOCK HOST] Sending GET_DESCRIPTOR (String) Setup Packet on EP0...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x03020680;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00FF0409;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_set_configuration(void) {
    uart_puts("\n[MOCK HOST] Sending SET_CONFIGURATION Setup Packet on EP0...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x00010900;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00000000;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_get_status(void) {
    uart_puts("\n[MOCK HOST] Sending unsupported GET_STATUS Setup Packet to test STALL...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x00000080;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00020000;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_send_ep1_bulk(void) {
    uart_puts("\n[MOCK HOST] Sending Bulk Data to EP1 OUT...\n");
    const char *mock_data = "HELLO WORLD!";
    for (int i = 0; i < 13; i++) {
        ep_rx_buffer[1][i] = mock_data[i];
    }
    USB_ENDPTPRIME &= ~(1 << 1);
    mock_hardware_endptcomplete |= (1 << 1);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_fetch_ep1_bulk(void) {
    uart_puts("\n[MOCK HOST] Fetching Bulk Data from EP1 IN...\n");
    USB_ENDPTPRIME &= ~(1 << 17);
    mock_hardware_endptcomplete |= (1 << 17);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_setup_packet(void) {
    uart_puts("\n[MOCK HOST] Sending GET_DESCRIPTOR Setup Packet on EP0...\n");
    endpoint_list[EP_OUT_IDX(0)].setup_buf[0] = 0x01000680;
    endpoint_list[EP_OUT_IDX(0)].setup_buf[1] = 0x00120000;
    mock_hardware_endptsetupstat |= (1 << 0);
    mock_hardware_usbsts |= USBSTS_UI;
}

void mock_virtual_host_reset(void) {
    uart_puts("\n[MOCK HOST] Simulating physical cable plug-in and Bus Reset...\n");
    mock_hardware_usbsts |= USBSTS_URI; 
}

void usb_poll(void) {
    uint32_t status = USB_USBSTS | mock_hardware_usbsts;

    if (status & USBSTS_URI) {
        uart_puts("\n[EVENT] USB Bus Reset Received!\n");
        device_state = STATE_DEFAULT;
        USB_ENDPTSETUPSTAT = USB_ENDPTSETUPSTAT;
        mock_hardware_endptsetupstat = 0;
        USB_USBSTS = USBSTS_URI; 
        mock_hardware_usbsts &= ~USBSTS_URI;
        mock_virtual_host_setup_packet();
    }
    
    if (status & USBSTS_UI) {
        uart_puts("\n[EVENT] USB General Interrupt (Setup or Transfer) Received!\n");
        USB_USBSTS = USBSTS_UI;
        mock_hardware_usbsts &= ~USBSTS_UI;

        uint32_t setup_stat = USB_ENDPTSETUPSTAT | mock_hardware_endptsetupstat;
        if (setup_stat & (1 << 0)) {
            handle_ep0_setup();
        }

        uint32_t complete_stat = USB_ENDPTCOMPLETE | mock_hardware_endptcomplete;
        if (complete_stat & (1 << 16)) { 
            uart_puts("\n[EVENT] EP0 IN Transfer Complete!\n");
            USB_ENDPTCOMPLETE = (1 << 16);
            mock_hardware_endptcomplete &= ~(1 << 16);

            if (device_state == STATE_ADDRESS_PENDING) {
                uart_puts("[INFO] SET_ADDRESS Status Phase Complete. Applying new address now.\n");
                USB_DEVICEADDR = (pending_address << 25);
                device_state = STATE_ADDRESSED;
                uart_puts("[INFO] Device Address is now: ");
                uart_print_hex(pending_address); uart_puts("\n");
                uart_puts("[INFO] SET_ADDRESS complete. Mocking host GET_DESCRIPTOR (Configuration)...\n");
                mock_virtual_host_get_config_descriptor();
            } else if (device_state == STATE_DEFAULT) {
                uart_puts("[INFO] GET_DESCRIPTOR (Device) complete. Mocking host SET_ADDRESS request...\n");
                mock_virtual_host_set_address();
            } else if (device_state == STATE_ADDRESSED) {
                uart_puts("[INFO] GET_DESCRIPTOR (Configuration) complete. Mocking host GET_DESCRIPTOR (String)...\n");
                device_state = STATE_CONFIG_FETCHED;
                mock_virtual_host_get_string_descriptor();
            } else if (device_state == STATE_CONFIG_FETCHED) {
                uart_puts("[INFO] GET_DESCRIPTOR (String) complete. Mocking host SET_CONFIGURATION...\n");
                mock_virtual_host_set_configuration();
            } else if (device_state == STATE_CONFIGURED) {
                uart_puts("\n[SUCCESS] ENUMERATION COMPLETE! Device is now in the Configured State.\n");
                mock_virtual_host_get_status();
            }
        }
        
        if (complete_stat & (1 << 1)) { 
            uart_puts("\n[EVENT] EP1 OUT Transfer Complete!\n");
            USB_ENDPTCOMPLETE = (1 << 1);
            mock_hardware_endptcomplete &= ~(1 << 1);
            uart_puts("[INFO] Bulk data received in ep1_rx_buffer: ");
            uart_puts((const char *)ep_rx_buffer[1]);
            uart_puts("\n");
            uart_puts("[INFO] Echoing data back to host via EP1 IN...\n");
            for(int i = 0; i < 13; i++) {
                ep_tx_buffer[1][i] = ep_rx_buffer[1][i];
            }
            prime_ep1_in(13);
            mock_virtual_host_fetch_ep1_bulk();
        }

        if (complete_stat & (1 << 17)) { 
            uart_puts("\n[EVENT] EP1 IN Transfer Complete!\n");
            USB_ENDPTCOMPLETE = (1 << 17);
            mock_hardware_endptcomplete &= ~(1 << 17);
            uart_puts("[INFO] Bulk data successfully sent to host!\n");
        }
    }
}