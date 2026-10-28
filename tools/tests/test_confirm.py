"""Host proofs of P6.27 / P6.28: the Yes / No window's state machine
(port/options/confirm.c) and its artwork in five languages
(platforms/pc/src/title_overlay_art.c)."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "platforms" / "pc" / "src"
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def confirm_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("confirm") / "confirm_test"
    cmd = [
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-DDW3_PORT_CONFIRM_TEST", "-o", str(out),
        str(TESTS / "confirm_main.c"), str(ROOT / "port" / "options" / "confirm.c"),
        f"-I{ROOT / 'port' / 'include'}", f"-I{ROOT / 'port' / 'memcard'}",
        f"-I{SRC}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module", params=["no-cheats", "cheats"])
def dialog_art_bin(request, tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("dialog_art") / "dialog_art_test"
    defs = ["-DDW3_CHEATS"] if request.param == "cheats" else []
    cmd = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", *defs,
        "-o", str(out), str(TESTS / "dialog_art_main.c"),
        str(SRC / "title_overlay_art.c"),
        str(ROOT / "port" / "pages" / "pages.c"), str(ROOT / "port" / "pages" / "glyph_atlas.c"), f"-I{SRC}", f"-I{ROOT / 'port' / 'include'}", "-lm",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_confirm_state_machine(confirm_bin: Path):
    r = subprocess.run([str(confirm_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: confirm state machine" in r.stdout
    assert "dw3-port-quit: saves flushed; asking the host to quit" in r.stderr
    assert "dw3-port-quit: save flush failed; quit cancelled" in r.stderr
    assert "dw3-port-confirm: return to title confirmed" in r.stderr


def test_dialog_and_title_art(dialog_art_bin: Path, tmp_path: Path):
    r = subprocess.run([str(dialog_art_bin)], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: dialog and title artwork" in r.stdout


def test_dialog_art_writes_evidence(dialog_art_bin: Path, tmp_path: Path):
    """With a directory the tool writes every window and title state."""
    r = subprocess.run([str(dialog_art_bin), str(tmp_path)], capture_output=True,
                       text=True, timeout=120)
    assert r.returncode == 0, r.stderr + r.stdout
    names = {p.name for p in tmp_path.glob("*.ppm")}
    assert "dialog-fr-return-yes.ppm" in names
    assert "title-es-3.ppm" in names
    assert len(names) == 5 * (4 + 4 + 5)


def test_options_art_selector_moved_to_five():
    """The Port options screen is art state 4 since the title gained a fourth
    row (DW3_TITLE_ART_OPTIONS); the hosts' atlas order follows it."""
    header = (SRC / "title_overlay_art.h").read_text()
    assert "DW3_TITLE_MENU_ROWS = 5, DW3_TITLE_ART_OPTIONS = 5" in header
    for host in (SRC / "title_menu_overlay.c",
                 ROOT / "platforms/switch/psyz_backend/nx_title_overlay.c"):
        text = host.read_text()
        assert "STATE_COUNT = 7" in text and "STATE_OPTIONS = 6" in text, host.name
