# Reset entry: kernel stack, clear .bss, install the trap vector, enter C.
    .section .text.start
    .globl _start
_start:
    la sp, __kstack_top
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:  la t0, trap_entry
    csrw mtvec, t0
    call kmain
3:  j 3b
