"""P4.3 / #188: the audio health counters and the `dw3-audio: underruns ...`
line (port/audio/audio_health.c)."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_audio_health_line(tmp_path: Path):
    out = tmp_path / "audio_health_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-o", str(out),
           str(ROOT / "tools" / "tests" / "audio_health_main.c"),
           str(ROOT / "port" / "audio" / "audio_health.c"),
           f"-I{ROOT / 'port' / 'audio'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "OK: audio health counters" in run.stdout


def test_hooks_are_wired():
    patch = (ROOT / "port" / "patches" /
             "0178-PORT_HOST-p43-keyon-result-health.patch").read_text()
    assert patch.count("dw3_port_audio_note_keyon(voice)") == 2
    nx = (ROOT / "platforms" / "switch" / "psyz_backend" / "nx_audio.c").read_text()
    assert "dw3_host_audio_queue_window" in nx
    assert "dw3_port_audio_health_exit()" in nx
    bus = (ROOT / "port" / "audio" / "audio_bus.c").read_text()
    assert "health_poll();" in bus
