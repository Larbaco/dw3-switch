"""Host proof of the deferred drawing in PSY-Z's CPU rasterizer (P6.22 pass 2)."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
PSYZ = ROOT / "vendor" / "psyz" / "psyz"
RASTER_C = PSYZ / "src" / "platform" / "sdl3_software.c"
DEFER_INC = PSYZ / "src" / "platform" / "sdl3_software_defer.inc"
TEST_C = ROOT / "tools" / "tests" / "defer_raster_main.c"


@pytest.fixture(scope="module")
def defer_raster_test(tmp_path_factory) -> Path:
    if not DEFER_INC.is_file():
        pytest.skip("vendor/psyz submodule is not at the deferred drawing revision")
    out = tmp_path_factory.mktemp("defer_raster") / "defer_raster_test"
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


def test_deferred_drawing_matches_immediate(defer_raster_test: Path):
    r = subprocess.run([str(defer_raster_test)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: software rasterizer deferred drawing" in r.stdout
