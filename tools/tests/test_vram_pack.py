"""Host proof that the VRAM rectangle pack matches a full-buffer RGB555 expand."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
PACK_C = ROOT / "platforms" / "switch" / "psyz_backend" / "nx_vram_pack.c"
TEST_C = ROOT / "tools" / "vram_pack_test.c"


@pytest.fixture(scope="module")
def vram_pack_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("vram_pack_build")
    out = build_dir / "vram_pack_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(TEST_C),
        str(PACK_C),
        f"-I{ROOT / 'platforms' / 'switch' / 'psyz_backend'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    return out


def test_rgb555_rect_matches_full_expand(vram_pack_test: Path):
    r = subprocess.run([str(vram_pack_test)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: rgb555 rect pack matches full-buffer expand" in r.stdout
    assert "OK: rgb555 wide pack puts the gutters beside the display" in r.stdout


GPU_TEST_C = ROOT / "tools" / "present16_gpu_test.c"
BACKEND = ROOT / "platforms" / "switch" / "psyz_backend"


def test_present16_shaders_match_the_pack(tmp_path_factory):
    """P6.22 pass 4: the real 16-bit present shaders on a GL ES 3 context
    (Mesa, surfaceless EGL) draw what the RGBA8888 pack draws, for all 65 536
    halfwords, at several scales, with the overlay's keep box and the wide
    texture. Skipped when this machine has no EGL/GLES3 or no usable context."""
    out = tmp_path_factory.mktemp("present16_gpu") / "present16_gpu_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o",
           str(out), str(GPU_TEST_C), str(PACK_C), f"-I{BACKEND}",
           "-lEGL", "-lGLESv2"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        pytest.skip("no EGL/GLES3 development files: " + r.stderr[-200:])
    env = dict(os.environ, EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1")
    r = subprocess.run([str(out)], capture_output=True, text=True, env=env)
    if r.returncode == 77:
        pytest.skip("no surfaceless GL ES 3 context: " + r.stderr[-200:])
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: 16-bit present is pixel-identical to the RGBA8888 pack" in r.stdout
    assert "all 65536 halfwords, opaque program" in r.stdout
