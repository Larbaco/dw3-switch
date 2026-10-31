"""Host proof of the retained Port options canvas shared by the PC and Switch
title overlays: dirty-region updates match a full reference render."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "platforms" / "pc" / "src"
TEST_C = ROOT / "tools" / "tests" / "options_cache_main.c"


@pytest.fixture(scope="module", params=["no-cheats", "cheats"])
def options_cache_test(request, tmp_path_factory) -> Path:
    """Built without and with DW3_CHEATS (P6.20: the Cheats page and the
    Square/Y hint go through the same cache)."""
    out = tmp_path_factory.mktemp("options_cache") / "options_cache_test"
    defs = ["-DDW3_CHEATS"] if request.param == "cheats" else []
    cmd = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", *defs,
        "-o", str(out),
        str(TEST_C), str(SRC / "title_options_cache.c"),
        str(SRC / "title_overlay_art.c"), str(ROOT / "tools" / "tests" / "port_version_stub.c"),
        str(ROOT / "port" / "pages" / "pages.c"), str(ROOT / "port" / "pages" / "glyph_atlas.c"),
        f"-I{ROOT / 'port' / 'include'}", "-pthread", "-lm",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_dirty_regions_match_reference(options_cache_test: Path):
    r = subprocess.run([str(options_cache_test)], capture_output=True, text=True,
                       timeout=600)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: options cache matches the reference render" in r.stdout
