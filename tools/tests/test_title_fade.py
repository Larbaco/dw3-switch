"""P38: the title edge fade must not darken the in-game Options screen (black square)."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_title_fade_rules(tmp_path):
    out = tmp_path / "title_fade"
    cmd = [
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
        str(ROOT / "tools" / "tests" / "title_fade_main.c"),
        f"-I{ROOT / 'platforms' / 'switch' / 'psyz_backend'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed = dict(line.split("=", 1) for line in run.stdout.splitlines())
    assert parsed, "no output"
    for key, value in parsed.items():
        assert value == "1", key
