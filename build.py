"""Build a moss image and run it on the flint ISS or RTL.

The kernel and the user programs link into one flat binary. Which programs
start at boot is decided here: build.py writes a small C table and compiles
it with the rest.
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FLINT_DIR = Path(os.environ.get("FLINT_DIR", ROOT.parent / "flint-traps"))
sys.path.insert(0, str(FLINT_DIR))

from flint.iss import Iss  # noqa: E402
from flint import rtlsim  # noqa: E402

TARGET = ["-target", "riscv32-freestanding", "-mcpu=generic_rv32", "-mno-relax"]
BUILD = ROOT / "build"


def sources():
    return sorted(
        [*(ROOT / "kernel").glob("*.c"), *(ROOT / "kernel").glob("*.S"),
         *(ROOT / "user").glob("*.c"), *(ROOT / "user").glob("*.S")]
    )


def boot_table(programs):
    lines = ['#include "kernel.h"']
    lines += [f"int {p}_main(void);" for p in programs]
    lines.append("const struct program boot_programs[] = {")
    lines += [f'    {{"{p}", {p}_main}},' for p in programs]
    lines.append("};")
    lines.append(f"const int boot_count = {len(programs)};")
    return "\n".join(lines) + "\n"


def build(programs, name="image", defines=None, opt="-O2"):
    """Compile and return the raw image bytes."""
    BUILD.mkdir(exist_ok=True)
    table = BUILD / f"{name}_boot.c"
    table.write_text(boot_table(programs))
    elf, bin_ = BUILD / f"{name}.elf", BUILD / f"{name}.bin"
    flags = [f"-D{k}={v}" for k, v in (defines or {}).items()]
    cmd = [
        sys.executable, "-m", "ziglang", "cc", *TARGET, "-nostdlib",
        "-ffreestanding", opt, "-fno-sanitize=all", "-Wall",
        f"-I{ROOT / 'kernel'}", f"-I{ROOT / 'user'}", *flags,
        f"-Wl,-T,{ROOT / 'kernel' / 'link.ld'}",
        *map(str, sources()), str(table), "-o", str(elf),
    ]
    subprocess.run(cmd, check=True)
    subprocess.run([sys.executable, "-m", "ziglang", "objcopy", "-O", "binary",
                    str(elf), str(bin_)], check=True)
    return bin_.read_bytes()


def run_iss(image, max_steps=2_000_000):
    iss = Iss(image)
    log = iss.run(max_steps=max_steps)
    return iss.output.decode(), iss.exit_code, len(log)


def run_rtl(image, max_cycles=4_000_000):
    res = rtlsim.run(image, max_cycles=max_cycles)
    return res.output.decode() if isinstance(res.output, (bytes, bytearray)) \
        else res.output, res.exit_code, res.cycles, res.retired


if __name__ == "__main__":
    progs = sys.argv[1:] or ["hello", "ping", "pong", "files", "sh"]
    out, code, steps = run_iss(build(progs))
    print(out, end="")
    print(f"-- exit {code}, {steps} instructions")
