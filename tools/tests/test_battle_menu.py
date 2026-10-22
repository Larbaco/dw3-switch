"""P7.9 battle menu memory (port/battle_menu): what the command menu and the
technique list start on, per mode, and that Off changes nothing."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def menu_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("battle_menu") / "battle_menu_test"
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_PORT_BATTLE_MENU_TEST", "-o", str(out),
           str(TESTS / "battle_menu_main.c"),
           str(ROOT / "port" / "battle_menu" / "battle_menu_memory.c"),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_battle_menu_policy(menu_bin: Path):
    r = subprocess.run([str(menu_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: battle menu memory policy" in r.stdout


def test_battle_menu_log_lines(menu_bin: Path):
    err = subprocess.run([str(menu_bin)], capture_output=True, text=True).stderr
    assert "dw3-battle-menu: remembered command Techniques" in err
    assert "dw3-battle-menu: command menu opens on Techniques (remembered)" in err
    assert "dw3-battle-menu: remembered technique 54 for partner 0" in err
    assert "technique list of partner 0 opens on technique 54 (page 0, line 1, remembered)" in err
    # Run and Digivolve are never reported as remembered
    assert "remembered command Run" not in err
    assert "remembered command Digivolve" not in err


def test_patch_0067_is_only_port_host():
    """Every hook of the patch sits inside PORT_HOST, so a non-host build of the
    decomp is untouched."""
    text = (ROOT / "port" / "patches" / "0067-PORT_HOST-p79-battle-menu-memory.patch").read_text()
    assert text.startswith("Upstream-bound: no\n")
    added = [l[1:] for l in text.splitlines() if l.startswith("+") and not l.startswith("+++")]
    assert any("dw3_port_battle_menu_" in l for l in added)
    # no hunk adds code outside an #if PORT_HOST: the added lines that call the
    # port are always inside one (checked by looking back for the nearest #if)
    lines = text.splitlines()
    for n, l in enumerate(lines):
        if l.startswith("+") and "dw3_port_battle_menu_" in l:
            before = [x[1:] if x[:1] in "+ " else x for x in lines[:n]]
            depth_if = [x for x in before if x.startswith(("#if PORT_HOST", "#endif", "#else"))]
            assert depth_if and depth_if[-1].startswith("#if PORT_HOST"), l
