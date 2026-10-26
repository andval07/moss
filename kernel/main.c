// Boot and the C side of the trap handler.

#include "kernel.h"

void kmain(void) {
    page_init();
    fs_init();
    kprintf("moss: %d pages of %d bytes\n", pages_total(), PAGE_SIZE);
    for (int i = 0; i < boot_count; i++) {
        int pid = proc_spawn(&boot_programs[i]);
        if (pid < 0) kprintf("spawn %s failed: %d\n", boot_programs[i].name, pid);
    }
    csr_write(mie, MIE_MTIE);
    csr_write(mstatus, MSTATUS_MPIE);  // MPP = user, interrupts on after mret
    trap_return(schedule(1));
}

// tf is the frame of the process that trapped (current). Returns the frame
// of the process to run next.
struct trapframe *trap(struct trapframe *tf) {
    uint32_t cause = csr_read(mcause);
    if (cause == CAUSE_TIMER) {
        timer_arm();
        wake_sleepers();
        return schedule(1);
    }
    if (cause == CAUSE_ECALL_U) {
        tf->pc += 4;
        return schedule(syscall(tf));
    }
    kprintf("[%s pid %d killed: cause %u pc %x tval %x]\n", current->name,
            current->pid, cause, tf->pc, csr_read(mtval));
    proc_exit(current, -1);
    return schedule(1);
}
