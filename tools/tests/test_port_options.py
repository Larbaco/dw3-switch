"""Headless checks for the P9.2 port-options open/close/tick shell."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module", params=["pc", "switch_glyphs_row"])
def port_options_bin(tmp_path_factory, request) -> Path:
    """The PC build (three Controls rows) and the build that lists the Switch
    glyphs row (P10.6, what a Switch build has)."""
    out = tmp_path_factory.mktemp("port_options") / "port_options_test"
    extra = ["-DDW3_FORCE_GLYPHS_ROW"] if request.param == "switch_glyphs_row" else []
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST",
        *extra,
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "port_options_main.c"),
        str(ROOT / "port" / "options" / "port_options.c"),
        str(ROOT / "port" / "options" / "button_map.c"),
        str(ROOT / "port" / "options" / "options_cfg.c"),
        f"-I{ROOT / 'port' / 'include'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    return out


def test_port_options_open_close_tick(port_options_bin: Path):
    run = subprocess.run([str(port_options_bin)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    assert "OK: port_options state machine" in run.stdout
    assert "dw3-port-options: open" in run.stderr
    assert "dw3-port-options: close" in run.stderr
    assert "widescreen_default=1" in run.stdout
    # P6.7 phase 3: Português (BR) is choosable with no text pack; the port screens follow the row
    assert "dw3-port-options: text language ptbr" in run.stderr
    assert "dw3-l10n: port screens in Portuguese (BR) (text_language=ptbr, game text original (no usable text pack))" in run.stderr
    assert "dw3-l10n: port screens in Portuguese (BR) (text_language=ptbr, game text from the pack)" in run.stderr
    assert "dw3-l10n: port screens in the disc's language (text_language=original, game text original)" in run.stderr
    assert "refused" not in run.stderr


def test_port_glyph_lsb_left(tmp_path: Path):
    """port_glyph.h uses the same LSB-left convention as the debug HUD font."""
    src = tmp_path / "glyph_main.c"
    src.write_text(
        """
#include "port_glyph.h"
#include <stdio.h>
int main(void) {
    if (!dw3_port_glyph_b_row0_ltr()) return 1;
    if (!dw3_port_glyph_pixel(0x01u, 0)) return 2;
    if (dw3_port_glyph_pixel(0x01u, 7)) return 3;
    printf("OK: port_glyph lsb-left\\n");
    return 0;
}
"""
    )
    out = tmp_path / "glyph"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(src),
        f"-I{ROOT / 'port' / 'include'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    assert "OK: port_glyph" in run.stdout
