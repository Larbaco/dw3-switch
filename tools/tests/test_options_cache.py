"""Host proof of the retained Port options canvas shared by the PC and Switch
title overlays: dirty-region updates match a full reference render."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "platforms" / "pc" / "src"
TEST_C = ROOT / "tools" / "tests" / "options_cache_main.c"


@pytest.fixture(scope="module")
def options_cache_test(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("options_cache") / "options_cache_test"
    cmd = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-o", str(out),
        str(TEST_C), str(SRC / "title_options_cache.c"),
        str(SRC / "title_overlay_art.c"),
        "-lm",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_dirty_regions_match_reference(options_cache_test: Path):
    r = subprocess.run([str(options_cache_test)], capture_output=True, text=True,
                       timeout=600)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: options cache matches the reference render" in r.stdout
