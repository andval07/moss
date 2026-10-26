#ifndef MOSS_KERNEL_H
#define MOSS_KERNEL_H

#include <stdarg.h>
#include <stdint.h>

#include "../user/abi.h"

// --- machine -----------------------------------------------------------------

#define TOHOST    ((volatile uint32_t *)0x10000000)
#define PUTCHAR   ((volatile uint32_t *)0x10000004)
#define MTIMECMP  ((volatile uint32_t *)0x02004000)
#define MTIME     ((volatile uint32_t *)0x0200bff8)
#define RAM_END   0x10000u

#define MSTATUS_MPIE 0x80u
#define MIE_MTIE     0x80u

#define CAUSE_ILLEGAL  2u
#define CAUSE_ECALL_U  8u
#define CAUSE_TIMER    0x80000007u

#define csr_read(name) ({ uint32_t v_; \
    __asm__ volatile("csrr %0, " #name : "=r"(v_)); v_; })
#define csr_write(name, v) __asm__ volatile("csrw " #name ", %0" :: "r"(v))

// One time slice, counted in retired instructions like mtime. Also the unit
// of sleep(). A power of two, so no multiply is needed.
#ifndef TICK_SHIFT
#define TICK_SHIFT 10
#endif
#define TICK (1u << TICK_SHIFT)

// --- processes ---------------------------------------------------------------

#define NPROC  8
#define NOFILE 4

enum proc_state { UNUSED, RUNNABLE, SLEEPING };

// Same layout as trap.S: slot 0 holds the pc (x0 needs no slot), slot i
// holds x[i], so x[2] is sp and x[10] is a0.
struct trapframe {
    union {
        uint32_t pc;
        uint32_t x[32];
    };
};

struct ofile;

struct proc {
    struct trapframe tf;  // first: trap.S uses the proc address as the frame
    int pid;
    enum proc_state state;
    uint32_t wake;        // mtime to wake at, when SLEEPING
    void *stack;
    struct ofile *fd[NOFILE];
    const char *name;
};

struct program {
    const char *name;
    int (*entry)(void);
};

extern const struct program boot_programs[];
extern const int boot_count;

int proc_spawn(const struct program *prog);
void proc_exit(struct proc *p, int code);
struct trapframe *schedule(int give_up);
void timer_arm(void);
void wake_sleepers(void);
extern struct proc *current;

void trap_return(struct trapframe *tf) __attribute__((noreturn));
int syscall(struct trapframe *tf);

// --- memory ------------------------------------------------------------------

#define PAGE_SIZE 1024

void page_init(void);
void *page_alloc(void);
void page_free(void *page);
int pages_free(void);
int pages_total(void);

// --- files -------------------------------------------------------------------

void fs_init(void);
int fs_open(struct proc *p, const char *path, int flags);
int fs_read(struct proc *p, int fd, char *buf, uint32_t len);
int fs_write(struct proc *p, int fd, const char *buf, uint32_t len);
int fs_close(struct proc *p, int fd);
int fs_dup_console(struct proc *p);

// --- console -----------------------------------------------------------------

void kputs(const char *s);
void kprintf(const char *fmt, ...);
void halt(uint32_t code) __attribute__((noreturn));

// shared with user code
int vformat(char *buf, int cap, const char *fmt, va_list ap);
void *memset(void *dst, int c, uint32_t n);
void *memcpy(void *dst, const void *src, uint32_t n);
int strcmp(const char *a, const char *b);
uint32_t strlen(const char *s);

#endif
