"""P6.31 auto-save (off by default): the policy, the idle-frame state machine,
the write onto the card (target slot, the player's version kept aside, rollback
on a failed flush), the area table against the decomp, and the hooks of patches
0140 / 0141. Everything is synthetic: cards are built at test time in pytest
temp directories. No game data is read or written.
"""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
AUTOSAVE_C = ROOT / "port" / "autosave" / "autosave.c"
AREAS_C = ROOT / "port" / "autosave" / "autosave_areas.c"
HOST_C = ROOT / "port" / "memcard" / "memcard_host.c"
CONVERT_C = ROOT / "port" / "memcard" / "save_convert.c"
TEST_C = ROOT / "tools" / "tests" / "autosave_main.c"
STGMCARD_DATA = (ROOT / "vendor" / "dw3_decomp" / "src" / "menus" / "memory_card" /
                 "data" / "stgmcard.c")
PATCHES = ROOT / "port" / "patches"


@pytest.fixture(scope="module")
def autosave_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("autosave") / "autosave_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-DDW3_AUTOSAVE_TEST",
           "-I", str(ROOT / "port" / "include"), "-I", str(ROOT / "port" / "memcard"),
           "-o", str(out), str(TEST_C), str(AUTOSAVE_C), str(AREAS_C), str(HOST_C),
           str(CONVERT_C)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_policy_state_machine_and_write(autosave_bin: Path, tmp_path: Path):
    r = subprocess.run([str(autosave_bin), str(tmp_path)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: autosave" in r.stdout
    # The auto-save is its own 32 KiB card file; no .pre-auto copy exists.
    assert (tmp_path / "autosave.bin").stat().st_size == 0x8000
    assert not (tmp_path / "save0.bin.pre-auto").exists()


def decomp_area_table() -> dict[int, int]:
    text = STGMCARD_DATA.read_text()
    body = text.split("STGMCARD_prevModes[] = {")[1].split("};")[0]
    table: dict[int, int] = {}
    for area, _stage, mode in re.findall(
            r"\{\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*([A-Za-z_0-9]+)\s*\}", body):
        if mode.startswith("MODE_") or mode == "0x0":
            continue
        table[int(mode, 16)] = int(area, 16)  # the screen's loop keeps the last entry
    return table


def port_area_table() -> dict[int, int]:
    text = AREAS_C.read_text()
    return {int(m, 16): int(a, 16)
            for m, a in re.findall(r"\{0x([0-9A-F]{3}), 0x([0-9A-F]{2})\}", text)}


@pytest.mark.skipif(not STGMCARD_DATA.exists(), reason="vendor/dw3_decomp is not checked out")
def test_area_table_matches_the_decomp():
    ours = port_area_table()
    assert ours == decomp_area_table()
    assert len(ours) == 239


def test_area_table_is_sorted_for_the_binary_search():
    modes = [m for m in re.findall(r"\{0x([0-9A-F]{3}), 0x[0-9A-F]{2}\}", AREAS_C.read_text())]
    values = [int(m, 16) for m in modes]
    assert values == sorted(set(values))


def test_patch_0140_hook_is_off_by_default_and_leaves_game_alone():
    text = (PATCHES / "0140-PORT_HOST-p631-autosave-field-hook.patch").read_text()
    assert "Upstream-bound: no" in text
    # Everything is inside PORT_HOST and GAME is read, not written.
    added = [l[1:] for l in text.splitlines() if l.startswith("+") and not l.startswith("+++")]
    assert "dw3_autosave_field_idle(&autosave)" in "\n".join(added)
    assert not any(re.search(r"\bGAME\.\w+\s*=[^=]", l) for l in added), \
        "the hook must not assign to GAME (it saves a private copy)"
    assert "#if PORT_HOST" in "\n".join(added)


def test_patch_0141_hooks_are_notes_only():
    text = (PATCHES / "0141-PORT_HOST-p631-autosave-slot-notes.patch").read_text()
    added = "\n".join(l[1:] for l in text.splitlines()
                      if l.startswith("+") and not l.startswith("+++"))
    for call in ("dw3_autosave_note_loaded()", "dw3_autosave_title_started()"):
        assert call in added
    assert "note_saved" not in added


def test_patch_0142_continue_loads_the_autosave():
    text = (PATCHES / "0142-PORT_HOST-p631-autosave-continue-title.patch").read_text()
    added = "\n".join(l[1:] for l in text.splitlines()
                      if l.startswith("+") and not l.startswith("+++"))
    assert "dw3_autosave_load(autosave, GAME_SAVE_SIZE" in added
    assert "dw3_port_title_load_row()" in added
    assert "task->choice = 4;" in added
    assert "requestMode(GAME.fieldMode, 0)" in added
    # falls back to the slot list if the file cannot be read
    assert "requestMode(MODE_CONTINUE, 0)" in added
