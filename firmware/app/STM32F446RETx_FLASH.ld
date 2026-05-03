/* STM32F446RETx_FLASH.ld
 * Application linker script — starts at sector 4 (0x08010000).
 * Sectors 0-1 (0x08000000, 32 KB) are reserved for the bootloader.
 * Flash: 448KB @ 0x08010000 (sectors 4-7)
 * SRAM1: 112KB @ 0x20000000
 * SRAM2:  16KB @ 0x2001C000
 * (SRAM1 + SRAM2 = 128KB, mapped contiguously)
 */

ENTRY(Reset_Handler)

/* Minimum sizes for the C runtime (not FreeRTOS tasks).
 * FreeRTOS task stacks come from configTOTAL_HEAP_SIZE via heap_4.c. */
_Min_Heap_Size  = 0x400;   /* 1KB  */
_Min_Stack_Size = 0x800;   /* 2KB  */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08010000, LENGTH = 448K
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Initial stack pointer: top of SRAM (end of 128KB region = 0x20020000).
 * startup_stm32f446xx.s loads _estack into MSP as the first vector entry. */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* Interrupt vector table — must be first in Flash */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* Code and read-only data */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* ARM exception unwinding */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* Constructors / destructors */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* Initialized data: load address in Flash, runtime address in SRAM.
     * startup_stm32f446xx.s copies [_sidata .. _sidata+(_edata-_sdata)]
     * to [_sdata .. _edata] during Reset_Handler. */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.RamFunc)
        *(.RamFunc*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM AT> FLASH

    /* Zero-initialized data — startup zeroes [_sbss .. _ebss] */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >SRAM

    /* Heap and MSP stack at end of SRAM */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >SRAM

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
