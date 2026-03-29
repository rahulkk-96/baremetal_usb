ENTRY(_start)

MEMORY
{
    /* QEMU mcimx6ul-evk (i.MX6UL) standard DDR memory starts at 0x80000000 */
    RAM (xrw) : ORIGIN = 0x80000000, LENGTH = 256M
}

SECTIONS
{
    .text : {
        . = ALIGN(4);
        *(.text.boot)       /* Startup assembly must go first */
        *(.text)            /* Program code */
        *(.text.*)
    } > RAM

    .rodata : {
        . = ALIGN(4);
        *(.rodata)          /* Constant data */
        *(.rodata.*)
    } > RAM

    .data : {
        . = ALIGN(4);
        *(.data)
        *(.data.*)
    } > RAM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Dedicated DMA Section for USB Endpoint Lists */
    . = ALIGN(2048);
    .uncached_dma (NOLOAD) : {
        __uncached_start = .;
        *(.uncached_dma)
        . = . + 0x10000; /* Reserve 64KB for our uncached heap */
        . = ALIGN(2048);
        __uncached_end = .;
    } > RAM

    /* 16KB stack */
    . = ALIGN(8);
    . = . + 0x4000;
    __stack_top = .;
}
