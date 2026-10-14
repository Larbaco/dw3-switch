"""Host proof of the P6.1 widescreen side gutters in PSY-Z's CPU rasterizer."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
PSYZ = ROOT / "vendor" / "psyz" / "psyz"
RASTER_C = PSYZ / "src" / "platform" / "sdl3_software.c"
TEST_C = ROOT / "tools" / "tests" / "gutter_raster_main.c"


@pytest.fixture(scope="module")
def gutter_raster_test(tmp_path_factory) -> Path:
    if not RASTER_C.is_file():
        pytest.skip("vendor/psyz submodule not initialized")
    out = tmp_path_factory.mktemp("gutter_raster") / "gutter_raster_test"
    cmd = [
        "gcc",
        "-std=gnu11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(TEST_C),
        str(RASTER_C),
        f"-I{PSYZ / 'include'}",
        f"-I{PSYZ / 'src'}",
        f"-I{PSYZ / 'src' / 'platform'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_gutters_follow_display_draws(gutter_raster_test: Path):
    r = subprocess.run([str(gutter_raster_test)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: software rasterizer side gutters" in r.stdout
