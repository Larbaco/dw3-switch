"""Host proofs of P9.7 slice 2: the port Options screen in battle
(port/options/battle_options.c and patch 0165). docs/PORT_OPTIONS.md
"Options in battle"."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "platforms" / "pc" / "src"
OPTIONS = ROOT / "port" / "options"
TESTS = ROOT / "tools" / "tests"
PATCH = ROOT / "port" / "patches" / "0165-PORT_HOST-p97-battle-options-hold.patch"


@pytest.fixture(scope="module")
def battle_options_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("battle_options") / "battle_options_test"
    cmd = [
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
        "-DDW3_PORT_OPTIONS_TEST", "-DDW3_FIELD_OPTIONS_TEST", "-DDW3_BATTLE_OPTIONS_TEST",
        "-o", str(out),
        str(TESTS / "battle_options_main.c"), str(OPTIONS / "battle_options.c"),
        str(OPTIONS / "field_options.c"), str(OPTIONS / "port_options.c"),
        str(OPTIONS / "options_cfg.c"),
        f"-I{ROOT / 'port' / 'include'}", f"-I{SRC}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_battle_options_state_machine(battle_options_bin: Path):
    r = subprocess.run([str(battle_options_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: battle options" in r.stdout
    assert "open in game (battle command menu;" in r.stderr
    assert "close in game (back to the battle)" in r.stderr
    assert "resume after" in r.stderr
    assert "reset while held" in r.stderr


def test_patch_wires_entry_and_hold():
    """Patch 0165: the poll in the command menu's TASK_RUN and the hold branch
    in main(); the series table lists it."""
    patch = PATCH.read_text()
    assert "Upstream-bound: no" in patch
    assert "dw3_port_battle_options_poll(" in patch
    assert "dw3_port_battle_options_hold_frame()" in patch
    assert "dw3_port_battle_options_reset();" in patch
    assert "VSync(0);" in patch
    assert PATCH.name in (ROOT / "docs" / "PATCHES.md").read_text()
