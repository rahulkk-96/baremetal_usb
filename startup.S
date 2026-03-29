    .syntax unified
    .arm

    .global _start
    .extern __stack_top
    .extern __bss_start
    .extern __bss_end
    .extern main

    .section .text.boot

_start:
    /* 1. Initialize Stack Pointer */
    ldr sp, =__stack_top

    /* 2. Zero out the BSS section */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0

bss_clear_loop:
    cmp r0, r1
    bhs bss_clear_done      /* Branch if Higher or Same (unsigned) */
    strb r2, [r0], #1       /* Store Byte and post-increment by 1 */
    b bss_clear_loop

bss_clear_done:
    /* 3. Branch to main() */
    bl main

hang:
    /* 4. Infinite loop if main() returns */
    b hang
