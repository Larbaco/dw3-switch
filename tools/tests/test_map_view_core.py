"""Host proofs of the P6.32 phase 0 map view core (port/map_view/map_view_core.c):
the sprite-frame compose, the disc TIM archive load (RLEN included), the CPU
scalers (flat colour stays flat; FSR 1, Lanczos2, sharp prescale), the zoom and
pan maths and the cache file. Plain C, no game data."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def core_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("map_view_core") / "map_view_core_test"
    cmd = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-o", str(out),
        str(TESTS / "map_view_core_main.c"),
        str(ROOT / "port" / "map_view" / "map_view_core.c"),
        str(ROOT / "platforms" / "pc" / "src" / "title_logo_resample.c"),
        f"-I{ROOT / 'port' / 'map_view'}", f"-I{ROOT / 'platforms' / 'pc' / 'src'}",
        "-lm",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_map_view_core(core_bin: Path, tmp_path: Path):
    r = subprocess.run([str(core_bin), str(tmp_path)], capture_output=True,
                       text=True, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: map view core" in r.stdout


def test_map_view_core_compiles_as_c90(tmp_path: Path):
    """The port library is built as gnu89: the core must be valid there."""
    cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
           "-c", "-o", str(tmp_path / "core.o"),
           str(ROOT / "port" / "map_view" / "map_view_core.c"),
           f"-I{ROOT / 'port' / 'map_view'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
