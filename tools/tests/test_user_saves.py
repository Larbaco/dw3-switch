"""P6.26 per-user saves, slice 1: the user-resolution seam with a fake account
provider (none, one user, several, preselected, init fails, cancel, error),
the uid formatter, the per-user folder builder and the legacy copy. Synthetic
files only; no game data."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_user_saves(tmp_path: Path):
    exe = tmp_path / "user_saves_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-Wno-format-truncation",
           "-I", str(ROOT / "port" / "include"), "-o", str(exe),
           str(ROOT / "tools" / "tests" / "user_saves_main.c"),
           str(ROOT / "port" / "saves" / "user_saves.c")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    scratch = tmp_path / "scratch"
    scratch.mkdir()
    r = subprocess.run([str(exe), str(scratch)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: user_saves" in r.stdout
