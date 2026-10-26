"""Kernel behaviour, checked through what the processes print."""

import re

import pytest

import build


def run(programs, **defines):
    image = build.build(programs, name="t_" + "_".join(programs),
                        defines=defines)
    out, code, _ = build.run_iss(image)
    return out, code


def halt_pages(out):
    free, total = map(int, re.search(r"\[halt: (\d+)/(\d+)", out).groups())
    return free, total


def test_boot_and_exit_code():
    out, code = run(["hello"])
    assert out.startswith("moss: 46 pages of 1024 bytes\n")
    assert "hello from pid 1\n" in out
    assert "[hello pid 1 exit 0]\n" in out
    assert code == 0


def test_timer_preempts_programs_that_never_yield():
    out, _ = run(["ping", "pong"])
    lines = [l for l in out.splitlines() if l.startswith(("ping", "pong"))]
    assert sorted(lines) == sorted([f"{n} {i}" for n in ("ping", "pong")
                                    for i in range(4)])
    # without preemption all of ping would print before pong
    assert lines != [f"ping {i}" for i in range(4)] + [f"pong {i}" for i in range(4)]


def test_without_timer_the_first_program_runs_to_the_end():
    out, _ = run(["ping", "pong"], TICK_SHIFT=24)
    lines = [l for l in out.splitlines() if l.startswith(("ping", "pong"))]
    assert lines == [f"ping {i}" for i in range(4)] + [f"pong {i}" for i in range(4)]


def test_sleep_lets_others_run_first():
    out, _ = run(["sleeper", "hello"])
    assert out.index("hello from pid 2") < out.index("sleeper: awake")
    assert "[sleeper pid 1 exit 7]" in out


def test_files_and_error_codes():
    out, _ = run(["files"])
    data = bytes((i & 0xFF) for i in range(1500))
    assert f"files: read 1500 bytes, sum {sum(data)}\n" in out
    assert "files: after append 1504 bytes\n" in out
    assert "files: open missing = -2\n" in out      # ENOENT
    assert "files: write to read-only fd = -9\n" in out  # EBADF
    assert "files: read bad fd = -9\n" in out
    assert "files: bad pointer = -14\n" in out      # EFAULT


def test_file_pages_are_the_only_pages_left_in_use():
    out, _ = run(["files"])
    free, total = halt_pages(out)
    assert total - free == 2  # "data" is 1504 bytes: two pages
    out, _ = run(["hello", "ping", "pong"])
    free, total = halt_pages(out)
    assert free == total  # stacks all returned


def test_processes_share_files():
    out, _ = run(["writer", "reader"])
    assert "reader: 'note from the writer'\n" in out


def test_illegal_instruction_kills_only_that_process():
    out, code = run(["bad", "hello"])
    assert re.search(r"\[bad pid 1 killed: cause 2 pc [0-9a-f]+ tval 300024f3\]", out)
    assert "bad: still alive?" not in out
    assert "hello from pid 2" in out
    assert code == 0


def test_shell_script():
    out, _ = run(["sh"])
    assert "$ cat greet\nhello\nworld\n" in out
    assert "cat: missing: error 2\n" in out
    assert "sh: frobnicate: unknown command\n" in out


@pytest.mark.rtl
@pytest.mark.parametrize("programs", [["hello", "bad"], ["writer", "reader"],
                                      ["ping", "pong"]])
def test_rtl_matches_iss(programs):
    image = build.build(programs, name="rtl_" + "_".join(programs))
    out, code, _ = build.run_iss(image)
    rtl_out, rtl_code, _, _ = build.run_rtl(image)
    assert (rtl_out, rtl_code) == (out, code)
