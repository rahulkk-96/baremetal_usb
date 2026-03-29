# Bare-Metal USB 2.0 Enumeration

This project is a complete, from-scratch bare-metal USB 2.0 Device implementation targeting the **Synopsys Atlantic (Chipidea)** USB controller. 

It is designed to be fully simulated using QEMU's `mcimx6ul-evk` (NXP i.MX6UL) machine model, demonstrating a deep understanding of hardware-level USB state machines without requiring physical hardware.

## Features
- **Custom Startup & Linker Script:** Bare-metal ARM Cortex-A7 initialization and strict memory alignment for DMA structures.
- **DMA-driven Architecture:** Implements Device Queue Heads (dQH) and Device Transfer Descriptors (dTD) in uncached memory.
- **Full Enumeration State Machine:** Successfully handles `GET_DESCRIPTOR`, `SET_ADDRESS`, and `SET_CONFIGURATION` requests.
- **String Descriptors:** Implements USB UTF-16LE string descriptors for custom Manufacturer, Product, and Serial names.
- **Hardware STALLs:** Elegantly handles unsupported requests via hardware-level STALL handshakes.
- **Bidirectional Bulk Transfers:** Demonstrates an active "Echo Server" on Endpoint 1 (Bulk IN/OUT) utilizing DMA interrupts.
- **Mock Host Injection:** Includes a software mock host to simulate real-world USB Host behavior entirely within standard QEMU.

## Requirements
- `arm-none-eabi-gcc`
- `qemu-system-arm`
- `make`

## How to Run
Simply run the following command to compile the project and launch the QEMU simulation:

```bash
make run
```

To exit the QEMU emulator, press `Ctrl+A` followed by `X`.
