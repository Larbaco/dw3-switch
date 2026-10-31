"""Instant menu exit (port/menu_exit, patch 0200): the policy is on by default,
the file key and the variable decide, the screens are named as the field menu
lists them and the two log lines say what happened."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def menu_exit_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("menu_exit") / "menu_exit_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_MENU_EXIT_TEST", "-o", str(out),
           str(TESTS / "menu_exit_main.c"),
           str(ROOT / "port" / "menu_exit" / "menu_exit.c"),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_menu_exit_policy(menu_exit_bin: Path):
    r = subprocess.run([str(menu_exit_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: menu exit policy" in r.stdout
    assert ("dw3-menu: exit to field instant (skipped 96 transition frames, from items) "
            "time=0 play=0s+0 rng=0") in r.stderr
    assert ("dw3-menu: exit to field with the original transition (instant exit off, "
            "from map) time=0 play=0s+0 rng=0") in r.stderr
