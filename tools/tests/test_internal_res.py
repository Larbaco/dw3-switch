"""Headless checks for the P6.11 internal resolution policy (port/options/internal_res.c)."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def res_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("internal-res") / "internal_res_test"
    cmd = [
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST",
        "-DDW3_PORT_INTERNAL_RES_TEST", "-DVERSION_EU=0",
        "-o", str(out),
        str(ROOT / "tools" / "tests" / "internal_res_main.c"),
        str(ROOT / "port" / "options" / "internal_res.c"),
        str(ROOT / "port" / "options" / "port_options.c"),
        str(ROOT / "port" / "options" / "button_map.c"),
        str(ROOT / "port" / "options" / "options_cfg.c"),
        f"-I{ROOT / 'port' / 'include'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    return out


def run(binary: Path) -> subprocess.CompletedProcess[str]:
    env = {k: v for k, v in os.environ.items()
           if k not in ("DW3_INTERNAL_RES", "DW3_PC_INTERNAL_RES", "DW3_INTERNAL_RES_TURBO")}
    return subprocess.run([str(binary)], capture_output=True, text=True, env=env)


def test_internal_res_policy(res_bin: Path):
    result = run(res_bin)
    assert result.returncode == 0, result.stderr + result.stdout
    assert "OK: internal res policy" in result.stdout
    err = result.stderr
    # the log lines the OLED rows and the replays read (rule 12), once per event
    assert "dw3-res: set 2x (handheld)" in err
    assert "dw3-res: turbo on -> 1x" in err
    assert "dw3-res: turbo off -> 2x" in err
    assert err.count("dw3-res: clamp 3x -> 2x (handheld max)") == 1
    assert "dw3-res: set 3x (docked)" in err
    assert err.count("dw3-res: set 2x (handheld)") == 1
