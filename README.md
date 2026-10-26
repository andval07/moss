# moss

A small preemptive kernel for [flint](https://github.com/andval07/flint), my RV32I core. It runs
the same image on the Python instruction set simulator and on the Verilog
model, and the tests check that both print the same thing.

About 960 lines of C and assembly. The full image (kernel plus all demo
programs) is 13.6 KB.

## What it does

- Boot in machine mode, then run every process in user mode.
- One trap entry for interrupts, system calls and faults. It saves all 31
  registers in the trap frame of the current process and switches to a
  kernel stack.
- Up to 8 processes, round-robin. The machine timer preempts a process after
  1024 instructions (`TICK_SHIFT`), so a busy loop cannot hold the CPU.
- System calls: `exit`, `getpid`, `yield`, `sleep`, `open`, `read`, `write`,
  `close`. Errors come back as negative errno values (`ENOENT`, `EBADF`,
  `EFAULT`, ...).
- A page allocator: 46 pages of 1 KiB on a free list. Process stacks and
  file data come from it.
- A RAM filesystem: one flat directory, up to 8 files of up to 4 pages,
  with `O_CREAT`, `O_TRUNC` and `O_APPEND`.
- A process that runs an illegal instruction is killed; the others go on.
- Demo programs and a tiny shell (`echo`, `cat`, `>`/`>>`, `sleep`, `pid`).
  There is no keyboard, so the shell runs a fixed script.

## Layout

```
kernel/start.S    boot: stack, clear .bss, set mtvec
kernel/trap.S     trap entry and exit (the context switch)
kernel/main.c     kmain and the C trap handler
kernel/proc.c     processes, scheduler, timer
kernel/syscall.c  system call dispatch and pointer checks
kernel/page.c     page allocator
kernel/fs.c       RAM filesystem
kernel/lib.c      memset, printf-style formatting, software divide
user/             syscall stubs, printf, demo programs and the shell
build.py          compiles an image with a chosen set of boot programs
bench.py          cost measurements
```

## Context switch

While a process runs, `mscratch` holds the address of its trap frame. The
first instruction of the trap handler swaps `sp` and `mscratch`, so the
registers can be saved without touching the user stack. The C handler
returns the frame to resume. If it is a different process, that is the
switch: `trap_return` loads the new frame and runs `mret`.

## Numbers

Measured on the Verilog model with `python bench.py`. Each value is the
difference between two runs with 50 and 150 loop iterations, so boot and
exit cancel out. The time slice is set very long so no timer fires. The
values include the few instructions of the user loop around the call.

| Path | Cycles | Instructions |
|---|---|---|
| `getpid` (system call, no switch) | 210 | 167 |
| `yield` between two processes (system call + switch) | 280 | 232 |

Timer preemption, counted on the ISS from the interrupt to the `mret`:
209 to 286 instructions over 28 interrupts (it depends on how many slots
the scheduler scans and on whether a sleeper wakes up).

The core has no M extension, so `lib.c` has a software multiply and divide.
Most of the cost is the 31 loads and 31 stores of the trap frame plus the C
handler.

## Limits

- No memory protection. flint has no PMP, so a user process can write any
  RAM. The kernel only checks that system call buffers stay inside RAM, so a
  bad pointer cannot reach the MMIO registers.
- All programs are linked into the kernel image. There is no `exec` and no
  `fork`.
- When every process sleeps, the kernel spins until a timer deadline passes.
- Console input always returns end of file.

## Running

Needs the flint repository next to this one (or `FLINT_DIR`), `ziglang` from
pip and Icarus Verilog for the RTL tests.

```bash
python build.py hello ping pong files sh
python -m pytest -q
python bench.py
```

`python -m pytest -m "not rtl"` skips the Verilog runs.
