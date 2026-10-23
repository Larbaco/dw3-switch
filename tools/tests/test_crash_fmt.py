"""P5.4 crash report helpers: line formatter, ESR words, frame-pointer walk
and the log ring. The headers have no libnx dependency, so gcc can run them."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def crash_fmt_output(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("crash_fmt") / "crash_fmt"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "crash_fmt_main.c"),
        f"-I{ROOT / 'platforms' / 'switch' / 'psyz_backend'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        key, _, value = line.partition("=")
        parsed[key.strip()] = value.strip()
    return parsed


FORMAT = ["hex_min0", "hex_zero", "hex_pad16", "hex_max", "dec", "addr_in_text",
          "addr_outside", "addr_end_exclusive", "line_bounded", "str_null"]
CONTEXT = ["esr_rc6", "esr_write", "esr_iabort", "esr_none", "lang_fr", "lang_de",
           "lang_bad"]
WALK = ["walk_three", "walk_end", "walk_fp0", "walk_below_sp", "walk_straddle",
        "walk_wild", "walk_wild_high", "walk_misaligned", "walk_loop", "walk_self",
        "walk_null_lr", "walk_cap"]
REASON = ["reason_assert", "reason_assert_nofunc", "reason_assert_nulls",
          "reason_assert_cap", "reason_fatal", "reason_fatal_code", "reason_exit",
          "reason_exit_min", "basename_plain", "basename_trailing"]
RING = ["ring_empty", "ring_two", "ring_wrap_count", "ring_oldest", "ring_newest",
        "ring_torn_skipped", "ring_truncate"]


@pytest.mark.parametrize("case", FORMAT)
def test_line_format(crash_fmt_output, case):
    assert crash_fmt_output.get(case) == "1"


@pytest.mark.parametrize("case", CONTEXT)
def test_readable_context(crash_fmt_output, case):
    assert crash_fmt_output.get(case) == "1"


@pytest.mark.parametrize("case", WALK)
def test_frame_walk_never_leaves_the_stack(crash_fmt_output, case):
    assert crash_fmt_output.get(case) == "1"


@pytest.mark.parametrize("case", RING)
def test_log_ring(crash_fmt_output, case):
    assert crash_fmt_output.get(case) == "1"


@pytest.mark.parametrize("case", REASON)
def test_software_abort_reasons(crash_fmt_output, case):
    assert crash_fmt_output.get(case) == "1"
