// System calls. Number in a7, arguments in a0-a2, result back in a0.
// Blocking calls only change the process state; schedule() then picks
// someone else on the way out of the trap.

#include "kernel.h"

// No memory protection on this core, but the kernel still refuses buffers
// that leave RAM, so a bad pointer cannot reach the MMIO registers.
static int user_range_ok(uint32_t addr, uint32_t len) {
    return addr < RAM_END && len <= RAM_END - addr;
}

static int user_string_ok(uint32_t addr) {
    for (; addr < RAM_END; addr++)
        if (*(const char *)addr == '\0') return 1;
    return 0;
}

static int32_t sys_rw(struct trapframe *tf, int is_write) {
    uint32_t buf = tf->x[11], len = tf->x[12];
    if (!user_range_ok(buf, len)) return -EFAULT;
    int fd = (int)tf->x[10];
    if (is_write) return fs_write(current, fd, (const char *)buf, len);
    return fs_read(current, fd, (char *)buf, len);
}

// Returns 1 when the process gives up the CPU.
int syscall(struct trapframe *tf) {
    uint32_t *a = &tf->x[10];
    int32_t ret = 0;
    switch (tf->x[17]) {
    case SYS_EXIT:
        proc_exit(current, (int)a[0]);
        return 1;
    case SYS_GETPID:
        ret = current->pid;
        break;
    case SYS_YIELD:
        a[0] = 0;
        return 1;
    case SYS_SLEEP:
        current->wake = *MTIME + (a[0] << TICK_SHIFT);
        current->state = SLEEPING;
        break;
    case SYS_OPEN:
        ret = user_string_ok(a[0]) ? fs_open(current, (const char *)a[0], (int)a[1])
                                   : -EFAULT;
        break;
    case SYS_READ:
        ret = sys_rw(tf, 0);
        break;
    case SYS_WRITE:
        ret = sys_rw(tf, 1);
        break;
    case SYS_CLOSE:
        ret = fs_close(current, (int)a[0]);
        break;
    default:
        ret = -ENOSYS;
        break;
    }
    a[0] = (uint32_t)ret;
    return 0;
}
