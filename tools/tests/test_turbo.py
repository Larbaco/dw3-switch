"""Headless checks for the P6.4 fast-forward policy (port/options/turbo.c)."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module", params=["us", "eu"])
def turbo_bin(tmp_path_factory, request) -> Path:
    out = tmp_path_factory.mktemp(f"turbo-{request.param}") / "turbo_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DDW3_PORT_OPTIONS_TEST",
        "-DDW3_PORT_TURBO_TEST",
        f"-DVERSION_EU={int(request.param == 'eu')}",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "turbo_main.c"),
        str(ROOT / "port" / "options" / "turbo.c"),
        str(ROOT / "port" / "options" / "port_options.c"),
        str(ROOT / "port" / "options" / "options_cfg.c"),
        f"-I{ROOT / 'port' / 'include'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    return out


def run(binary: Path, **env: str) -> subprocess.CompletedProcess[str]:
    full_env = {k: v for k, v in os.environ.items() if k != "DW3_TURBO"}
    full_env.update(env)
    return subprocess.run(
        [str(binary)], capture_output=True, text=True, env=full_env
    )


def test_turbo_policy(turbo_bin: Path):
    result = run(turbo_bin)
    assert result.returncode == 0, result.stderr + result.stdout
    assert "OK: turbo policy" in result.stdout
    assert "dw3-turbo: on" in result.stderr
    assert "dw3-turbo: speed 2x" in result.stderr
    assert "dw3-turbo: off" in result.stderr


def test_turbo_env_starts_on(turbo_bin: Path):
    # DW3_TURBO=3 starts fast-forward on at 3x, so the first check fails.
    result = run(turbo_bin, DW3_TURBO="3")
    assert result.returncode == 1
    assert "FAIL: starts at normal speed" in result.stderr
