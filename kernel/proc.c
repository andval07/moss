// Process table and the round-robin scheduler.

#include "kernel.h"

extern void user_exit(void);  // usys.S: exit(a0), where main returns to

static struct proc procs[NPROC];
static int next_pid = 1;
struct proc *current;
static int current_slot = -1;

int proc_spawn(const struct program *prog) {
    struct proc *p = 0;
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].state == UNUSED) {
            p = &procs[i];
            break;
        }
    }
    if (!p) return -EMFILE;
    void *stack = page_alloc();
    if (!stack) return -ENOMEM;

    memset(p, 0, sizeof *p);
    p->pid = next_pid++;
    p->name = prog->name;
    p->stack = stack;
    p->tf.pc = (uint32_t)prog->entry;
    p->tf.x[1] = (uint32_t)user_exit;
    p->tf.x[2] = (uint32_t)stack + PAGE_SIZE;
    fs_dup_console(p);  // fd 0: stdin, always at end of file
    fs_dup_console(p);  // fd 1: stdout
    p->state = RUNNABLE;
    return p->pid;
}

void proc_exit(struct proc *p, int code) {
    for (int fd = 0; fd < NOFILE; fd++)
        if (p->fd[fd]) fs_close(p, fd);
    page_free(p->stack);
    p->state = UNUSED;
    kprintf("[%s pid %d exit %d]\n", p->name, p->pid, code);
}

void timer_arm(void) { *MTIMECMP = *MTIME + TICK; }

void wake_sleepers(void) {
    uint32_t now = *MTIME;
    for (int i = 0; i < NPROC; i++)
        if (procs[i].state == SLEEPING && (int32_t)(now - procs[i].wake) >= 0)
            procs[i].state = RUNNABLE;
}

// Next RUNNABLE process after current, in table order. When everyone sleeps
// there is no idle process: the kernel spins here, with interrupts off, until
// mtime passes the earliest wake time. mtime counts retired instructions, so
// the spin itself moves time forward.
static int pick(void) {
    for (;;) {
        int alive = 0;
        for (int n = 1; n <= NPROC; n++) {
            int slot = (current_slot + n) & (NPROC - 1);
            if (procs[slot].state == RUNNABLE) return slot;
            if (procs[slot].state == SLEEPING) alive = 1;
        }
        if (!alive) {
            kprintf("[halt: %d/%d pages free]\n", pages_free(), pages_total());
            halt(0);
        }
        wake_sleepers();
    }
}

// Called on the way out of every trap. A process that is still RUNNABLE
// keeps the CPU unless it gives it up (yield, timer) or cannot go on.
struct trapframe *schedule(int give_up) {
    if (give_up || !current || current->state != RUNNABLE) {
        int slot = pick();
        if (slot != current_slot) timer_arm();  // a fresh slice
        current_slot = slot;
        current = &procs[slot];
    }
    return &current->tf;
}
