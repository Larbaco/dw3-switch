"""Host proof that the VRAM rectangle pack matches a full-buffer RGB555 expand."""

from __future__ import annotations

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
