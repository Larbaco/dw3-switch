"""P9.8 button layout mapper (port/options/button_map.c): presets, permutation
rules, the permute of a raw pad word and the dw3-controls log lines."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
SRC = ROOT / "port" / "options" / "button_map.c"


@pytest.fixture(scope="module")
def bm_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("button_map") / "button_map_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_BUTTON_MAP_TEST", "-o", str(out),
           str(TESTS / "button_map_main.c"), str(SRC),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_button_map(bm_bin: Path):
    r = subprocess.run([str(bm_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: button map" in r.stdout


def test_button_map_log_lines(bm_bin: Path):
    err = subprocess.run([str(bm_bin)], capture_output=True, text=True).stderr
    # default identity first, then the cfg load of Nintendo
    assert "dw3-controls: map loaded layout=playstation source=default" in err
    assert ("dw3-controls: map cross<-B circle<-A triangle<-X square<-Y "
            "l1<-L r1<-R l2<-ZL r2<-ZR l3<-StickL r3<-StickR") in err
    assert "dw3-controls: map loaded layout=nintendo source=cfg" in err
    assert ("dw3-controls: map cross<-A circle<-X triangle<-B square<-Y "
            "l1<-L r1<-R l2<-ZL r2<-ZR l3<-StickL r3<-StickR") in err
    assert "dw3-controls: layout changed nintendo -> playstation (screen)" in err
    assert ("dw3-controls: hotkeys fast-forward=R3 (Right stick click) "
            "restart=L1+R1+Start+Select (L+R+Plus+Minus)") in err
    assert ("dw3-controls: map invalid (button_layout is not playstation or "
            "nintendo), using default") in err
    # once after boot, re-armed by every load or layout change
    assert err.count("first press source=B -> logical cross") >= 3
    assert err.count("first press source=A -> logical cross") >= 1


def test_button_map_compiles_as_c90(tmp_path: Path):
    """The port library is built as gnu89."""
    cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
           "-DDW3_BUTTON_MAP_TEST", "-c", "-o", str(tmp_path / "bm.o"),
           str(SRC), f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
