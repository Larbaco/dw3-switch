"""One fread of a field map, then copies. A second tile in the same frame
succeeds and does not set a busy flag.

No disc and no devkitPro: builds port/cdfs with the host tile test.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
CDFS_C = ROOT / "port" / "cdfs" / "cdfs.c"
CDFS_H = ROOT / "port" / "cdfs" / "cdfs.h"
TEST_C = ROOT / "tools" / "cdfs_tile_pace_test.c"


@pytest.fixture(scope="module")
def tile_pace_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("tile_pace_build")
    out = build_dir / "cdfs_tile_pace_test"
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
        str(CDFS_C),
        f"-I{ROOT / 'port' / 'cdfs'}",
        "-Wl,--wrap=fread",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    assert CDFS_H.is_file()
    return out


def test_one_map_fread_then_copies(tile_pace_test: Path):
    r = subprocess.run(
        [str(tile_pace_test)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: one map fread, later tiles are copies" in r.stdout
