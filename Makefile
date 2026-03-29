CROSS_COMPILE ?= arm-none-eabi-
CC      = $(CROSS_COMPILE)gcc

TARGET = usb_enum.elf
ARCH_FLAGS = -mcpu=cortex-a7 -marm
CFLAGS = $(ARCH_FLAGS) -O0 -g -Wall -Wextra -ffreestanding -nostdlib
LDFLAGS = -T linker.ld -nostdlib

SRCS_S = startup.S
SRCS_C = main.c usb.c uart.c
OBJS = $(SRCS_S:.S=.o) $(SRCS_C:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	qemu-system-arm -M mcimx6ul-evk -kernel $(TARGET) -nographic

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
