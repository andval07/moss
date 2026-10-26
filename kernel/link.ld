/* Flat 64 KiB. Code and data from 0, then pages for the allocator, then the
 * kernel stack at the top. */
ENTRY(_start)
MEMORY { RAM (rwx) : ORIGIN = 0, LENGTH = 64K }
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text*) } > RAM
    .rodata : { *(.rodata*) *(.srodata*) } > RAM
    .data : { *(.data*) *(.sdata*) } > RAM
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) __bss_end = .; } > RAM
    __pages_start = ALIGN(1024);
    __kstack_top = ORIGIN(RAM) + LENGTH(RAM);
    __pages_end = __kstack_top - 2048;
}
