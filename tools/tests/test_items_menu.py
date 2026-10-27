"""P6.33 items menu policy (port/items_menu): the partner cycle wraps, one
partner changes nothing, the option is on by default and the log line carries
the bag totals."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def items_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("items_menu") / "items_menu_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_ITEMS_MENU_TEST", "-o", str(out),
           str(TESTS / "items_menu_main.c"),
           str(ROOT / "port" / "items_menu" / "items_menu.c"),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_items_menu_policy(items_bin: Path):
    r = subprocess.run([str(items_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: items menu policy" in r.stdout
    assert ("dw3-items-menu: partner switch member 1 -> 2 (partner 4) "
            "bag=120 worn=7") in r.stderr
