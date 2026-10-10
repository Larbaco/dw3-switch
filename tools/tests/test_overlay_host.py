"""P2.6 host test: static overlay restore vs preserve semantics."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
HOST_C = ROOT / "port" / "overlays" / "overlay_host.c"
HOST_H = ROOT / "port" / "overlays" / "overlay_host.h"
TEST_C = ROOT / "tools" / "overlay_host_test.c"


@pytest.fixture(scope="module")
def overlay_host_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("overlay_build")
    out = build_dir / "overlay_host_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DPORT_HOST=1",
        "-o",
        str(out),
        str(TEST_C),
        str(HOST_C),
        f"-I{ROOT / 'port' / 'overlays'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    assert HOST_H.is_file()
    return out


def test_reset_vs_preserve(overlay_host_test: Path):
    r = subprocess.run(
        [str(overlay_host_test)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: overlay reset vs preserve" in r.stdout
