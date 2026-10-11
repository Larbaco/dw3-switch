"""Host checks for the Switch debug overlay text formatter."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def overlay_text_output(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("dbg_overlay") / "dbg_overlay"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "debug_overlay_text_main.c"),
        f"-I{ROOT / 'platforms' / 'switch' / 'source'}",
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


def test_fps_from_60_pump_window(overlay_text_output):
    assert overlay_text_output["fps60"] == "60.0"
    assert overlay_text_output["fps30"] == "30.0"
    assert overlay_text_output["fps0"] == "0.0"


def test_hud_lines_include_mesh_and_build(overlay_text_output):
    assert int(overlay_text_output["line_count"]) <= 12
    assert overlay_text_output["L0"] == "dw3 dbg"
    assert "fps ~60.0" in overlay_text_output["L1"]
    assert overlay_text_output["L2"] == "mode file=0x15a"
    assert overlay_text_output["L3"] == "mesh path=mesh+ui"
    assert "tris 10/8" in overlay_text_output["L4"]
    assert "nxlink" in overlay_text_output[f"L{int(overlay_text_output['line_count']) - 1}"]
    assert "mesh" in overlay_text_output[f"L{int(overlay_text_output['line_count']) - 1}"]
