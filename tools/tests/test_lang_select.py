"""P6.1 EU language select (patch 0187): Start or Cross confirms, Start wins a
tie, and the log line names the button."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def lang_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("lang_select") / "lang_select_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_LANG_SELECT_TEST", "-o", str(out),
           str(TESTS / "lang_select_main.c"),
           str(ROOT / "port" / "options" / "lang_select.c"),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_lang_select_confirm(lang_bin: Path):
    r = subprocess.run([str(lang_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: language select confirm" in r.stdout
    assert "dw3-lang-select: language 3 chosen by Cross (confirm)" in r.stderr
    assert "dw3-lang-select: language 2 chosen by Start" in r.stderr


def test_lang_select_compiles_as_c90(tmp_path: Path):
    """The port library is built as gnu89."""
    cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
           "-DDW3_LANG_SELECT_TEST", "-c", "-o", str(tmp_path / "ls.o"),
           str(ROOT / "port" / "options" / "lang_select.c"),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
