# System call stubs: number in a7, then ecall. Arguments are already in a0-a2.
#include "abi.h"

    .macro SYSCALL name, num
    .globl \name
\name:
    li a7, \num
    ecall
    ret
    .endm

    .text
    SYSCALL exit, SYS_EXIT
    SYSCALL getpid, SYS_GETPID
    SYSCALL yield, SYS_YIELD
    SYSCALL sleep, SYS_SLEEP
    SYSCALL open, SYS_OPEN
    SYSCALL read, SYS_READ
    SYSCALL write, SYS_WRITE
    SYSCALL close, SYS_CLOSE

# A program's main returns here: its return value is already in a0.
    .globl user_exit
user_exit:
    li a7, SYS_EXIT
    ecall
