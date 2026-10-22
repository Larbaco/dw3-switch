"""Host test for port/replay/dw3_replay.c, the P6.15 recording parser shared by
the PC host and the Switch replay build."""
from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_replay_parser_host(tmp_path):
    """Ordering, comments, no event cap, in-order checkpoints, missing file."""
    out = tmp_path / "replay_rec_test"
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
           str(ROOT / "tools/tests/replay_rec_main.c"),
           str(ROOT / "port/replay/dw3_replay.c"),
           f"-I{ROOT / 'port/include'}"]
    b = subprocess.run(cmd, capture_output=True, text=True)
    assert b.returncode == 0, b.stderr + b.stdout
    r = subprocess.run([str(out)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: replay parser" in r.stdout
