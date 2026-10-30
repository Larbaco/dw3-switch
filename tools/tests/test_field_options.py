"""Host proofs of P9.7 slice 1: the port Options screen from the field menu
(port/options/field_options.c and the in-game mode of port/options/port_options.c),
and the locked rows of the screen's artwork (platforms/pc/src/title_overlay_art.c).
docs/PORT_OPTIONS.md "Options in game"."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "platforms" / "pc" / "src"
OPTIONS = ROOT / "port" / "options"
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def field_options_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("field_options") / "field_options_test"
    cmd = [
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST", "-DDW3_FIELD_OPTIONS_TEST", "-o", str(out),
        str(TESTS / "field_options_main.c"), str(OPTIONS / "field_options.c"),
        str(OPTIONS / "port_options.c"), str(OPTIONS / "button_map.c"), str(OPTIONS / "options_cfg.c"),
        f"-I{ROOT / 'port' / 'include'}", f"-I{SRC}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_field_options_state_machine(field_options_bin: Path):
    r = subprocess.run([str(field_options_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: field options" in r.stdout
    assert "dw3-port-options: open in game" in r.stderr
    assert "dw3-port-options: Widescreen is locked in game (change it at the title)" in r.stderr
    assert "dw3-port-options: close in game" in r.stderr
    # Only Widescreen is refused; every other row logged its change.
    assert r.stderr.count("is locked in game") == 3  # Cross, Circle, the last Cross
    assert "music volume 7" in r.stderr and "encounter rate off" in r.stderr


def test_locked_rows_art(tmp_path: Path):
    """The Options window with Widescreen locked differs from the unlocked one
    only where the label, the value and the message line are, and the lock note
    exists in five languages."""
    out = tmp_path / "locked_art"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-DDW3_CHEATS",
           "-o", str(out), str(TESTS / "locked_art_main.c"),
           str(SRC / "title_overlay_art.c"), str(ROOT / "tools" / "tests" / "port_version_stub.c"),
        str(ROOT / "port" / "pages" / "pages.c"), str(ROOT / "port" / "pages" / "glyph_atlas.c"), str(SRC / "title_options_cache.c"),
           f"-I{SRC}", f"-I{ROOT / 'port' / 'include'}", "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    r = subprocess.run([str(out)], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: locked rows art" in r.stdout


def test_field_row_is_wired_in_the_patch():
    """Patch 0160 adds the row above Return to title and hands the pad to the
    screen while it is open; the series table lists it."""
    patch = (ROOT / "port" / "patches" / "0160-PORT_HOST-p97-field-menu-options-row.patch").read_text()
    assert "Upstream-bound: no" in patch
    assert "dw3_port_field_options_open(" in patch
    assert "dw3_port_field_options_tick();" in patch
    assert "TextWindow *options[8]" in patch
    assert "0160-PORT_HOST-p97-field-menu-options-row.patch" in (ROOT / "docs" / "PATCHES.md").read_text()
