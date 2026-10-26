# Trap entry and exit.
#
# While a process runs, mscratch holds the address of its trap frame:
#   0(frame) = pc, 4*i(frame) = x[i] for i >= 1
# On a trap the first instruction swaps sp with mscratch, so sp points at the
# frame and the user sp is parked in mscratch. All 31 registers go into the
# frame, then C runs on the kernel stack. trap() returns the frame of the
# process to resume, which may be a different one: that is the context switch.

    .text
    .globl trap_entry
    .align 2
trap_entry:
    csrrw sp, mscratch, sp
    sw x1, 4(sp)
    .irp n, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    sw x\n, (4*\n)(sp)
    .endr
    csrr t0, mscratch
    sw t0, 8(sp)
    csrr t0, mepc
    sw t0, 0(sp)
    mv a0, sp
    la sp, __kstack_top
    call trap

# a0 = frame to resume. Also the first entry into user mode.
    .globl trap_return
trap_return:
    csrw mscratch, a0
    lw t0, 0(a0)
    csrw mepc, t0
    mv sp, a0
    lw x1, 4(sp)
    .irp n, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    lw x\n, (4*\n)(sp)
    .endr
    lw sp, 8(sp)
    mret
