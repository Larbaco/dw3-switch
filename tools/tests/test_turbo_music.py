"""Headless checks for the P6.36 music tick gate (port/audio/turbo_music.c)."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module", params=["us", "eu"])
def tm_bin(tmp_path_factory, request) -> Path:
    out = tmp_path_factory.mktemp(f"turbo-music-{request.param}") / "turbo_music_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DDW3_TURBO_MUSIC_TEST",
        f"-DVERSION_EU={int(request.param == 'eu')}",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "turbo_music_main.c"),
        str(ROOT / "port" / "audio" / "turbo_music.c"),
        f"-I{ROOT / 'port' / 'include'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    return out


def test_turbo_music_gate(tm_bin: Path):
    result = subprocess.run([str(tm_bin)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr + result.stdout
    assert "OK: turbo music gate" in result.stdout


def test_turbo_music_log_lines(tm_bin: Path):
    err = subprocess.run([str(tm_bin)], capture_output=True, text=True).stderr
    before, _, after = err.partition("-- no music tick line above this one --")
    # Pass-through at 1x and with the row on Fast logs nothing at all.
    assert "music tick" not in before
    assert "fade scaled" not in before
    # The frame clock's edge lines: ticks per 60 game frames at 2x, 3x, 4x, 1.5x.
    assert re.search(r"music tick source=frames rate=30/s game_rate=2x", after)
    assert re.search(r"music tick source=frames rate=20/s game_rate=3x", after)
    assert re.search(r"music tick source=frames rate=15/s game_rate=4x", after)
    assert re.search(r"music tick source=frames rate=40/s game_rate=1\.5x", after)
    # Window lines carry the count the replay scenarios judge by.
    assert re.search(r"music tick window frames=60 ticks=15 wall_ms=", after)
    assert re.search(r"music tick source=game rate=60/s game_rate=1x", after)
    assert re.search(r"music tick source=wall rate=60/s game_rate=4x", after)
    assert re.search(r"music tick source=wall rate=50/s game_rate=4x", after)
    assert re.search(r"music tick debt dropped", after)
    assert "fade scaled 60 -> 15 ticks (game_rate=4x)" in after
    assert "fade scaled 60 -> 40 ticks (game_rate=1.5x)" in after
