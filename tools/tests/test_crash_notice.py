"""P5.4 PR 2 next-boot crash notice: text layout (wrapping, truncation,
countdown) and the integer scale for handheld / docked. The header has no
libnx dependency, so gcc can run it."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def notice_output(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("crash_notice") / "crash_notice"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "crash_notice_main.c"),
        f"-I{ROOT / 'platforms' / 'switch' / 'psyz_backend'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        key, _, value = line.partition("=")
        parsed[key.strip()] = value
    return parsed


CASES = ["title", "version", "reason", "mode", "stage", "path", "hint", "fits",
         "hint_two_digits", "wrap_first", "wrap_indent", "wrap_cut",
         "wrap_next_field", "wrap_fits", "hard_break", "hard_break_fits",
         "empty_version", "nonprint", "hint_zero", "scale_720", "scale_1080",
         "scale_tiny", "fit_720", "fit_1080", "secs_0", "secs_999", "secs_1000",
         "secs_7999", "secs_end"]


@pytest.mark.parametrize("case", CASES)
def test_notice_layout(notice_output, case):
    assert notice_output.get(case) == "1"


def test_notice_text_is_english_and_short(notice_output):
    lines = [v.split("|", 1)[1] for k, v in notice_output.items()
             if k.startswith("line")]
    assert lines[0] == "The last run crashed."
    assert "Report saved to" in lines
    assert all(len(text) <= 60 for text in lines)
