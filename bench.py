"""Measure what a system call and a context switch cost on the flint RTL.

Cycle counts come from the RTL simulation. Each number is a difference
between two runs that only change the loop count, so boot, spawn and exit
cancel out. The time slice is set very long, so no timer fires.
"""

import build
from flint.iss import Iss, Retired, Trap

QUIET = {"TICK_SHIFT": 24}
N1, N2 = 50, 150


def per_iteration(programs, label):
    runs = []
    for n in (N1, N2):
        image = build.build(programs, name=f"bench_{label}_{n}",
                            defines={**QUIET, "BENCH_N": n})
        out, code, cycles, retired = build.run_rtl(image)
        assert code == 0, out
        runs.append((cycles, retired))
    calls = (N2 - N1) * len(programs)
    cycles = (runs[1][0] - runs[0][0]) / calls
    insts = (runs[1][1] - runs[0][1]) / calls
    return cycles, insts


def timer_switch_path():
    """Instructions from a timer interrupt to the mret back into user mode,
    counted on the ISS. Returns (min, max, number of interrupts)."""
    image = build.build(["ping", "pong"], name="bench_timer")
    iss = Iss(image)
    log = iss.run(max_steps=2_000_000)
    mret = 0x30200073
    lengths = []
    for i, event in enumerate(log):
        if not (isinstance(event, Trap) and event.cause == 0x80000007):
            continue
        for n, e in enumerate(log[i + 1:], 1):
            if isinstance(e, Retired) and word(image, e.pc) == mret:
                lengths.append(n)
                break
    return min(lengths), max(lengths), len(lengths)


def word(image, pc):
    return int.from_bytes(image[pc:pc + 4], "little")


if __name__ == "__main__":
    c, i = per_iteration(["getpid_loop"], "getpid")
    print(f"getpid (syscall, no switch): {c:.1f} cycles, {i:.1f} instructions")
    c, i = per_iteration(["yielder", "yielder"], "yield")
    print(f"yield (syscall + switch):    {c:.1f} cycles, {i:.1f} instructions")
    lo, hi, n = timer_switch_path()
    print(f"timer interrupt to mret:     {lo}-{hi} instructions ({n} interrupts)")
