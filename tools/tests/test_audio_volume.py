"""P6.17: the Music / SFX volume step -> gain curve."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_volume_curve(tmp_path: Path):
    out = tmp_path / "audio_volume_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-o", str(out),
           str(ROOT / "tools" / "tests" / "audio_volume_main.c"),
           str(ROOT / "port" / "audio" / "volume.c"),
           f"-I{ROOT / 'port' / 'include'}", "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "OK: audio volume curve" in run.stdout
    assert "step 10 gain 32768 (0.0 dB)" in run.stdout
