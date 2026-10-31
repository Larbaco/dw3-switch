"""P6.38: restart into the chosen region. The platform independent helpers
(port/region/dw3_region_relaunch.c) and the Region at start row's restart window
end to end on the host (port/options/port_options.c as a combined image's game
copy, confirm.c, region_relaunch.c, with stand-ins for the pad and the host's
relaunch hooks)."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
REGION = ROOT / "port" / "region"
OPTIONS = ROOT / "port" / "options"
INCLUDE = ROOT / "port" / "include"
PC_SRC = ROOT / "platforms" / "pc" / "src"


def build(out: Path, *srcs: Path, defs=(), std="gnu11") -> Path:
    cmd = ["gcc", f"-std={std}", "-Wall", "-Wextra", "-Werror", "-O2", *defs, "-o", str(out),
           *map(str, srcs), f"-I{REGION}", f"-I{INCLUDE}", f"-I{TESTS}", f"-I{ROOT / 'port' / 'memcard'}",
           f"-I{PC_SRC}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def helpers_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("relaunch") / "relaunch_test", TESTS / "region_relaunch_main.c",
                 REGION / "dw3_region_relaunch.c", REGION / "dw3_disc.c", REGION / "dw3_region.c",
                 OPTIONS / "options_cfg.c")


@pytest.fixture(scope="module")
def row_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("rowrelaunch") / "region_row_test", TESTS / "region_row_main.c",
                 OPTIONS / "port_options.c", OPTIONS / "confirm.c", OPTIONS / "region_relaunch.c",
                 OPTIONS / "button_map.c", OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 REGION / "dw3_region.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST", "-DDW3_PORT_CONFIRM_TEST",
                       "-DDW3_REGION_RELAUNCH_TEST", "-DDW3_REGION_IMAGE=1"], std="gnu11")


def test_relaunch_helpers(helpers_bin: Path, tmp_path: Path):
    r = subprocess.run([str(helpers_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: region relaunch helpers" in r.stdout


def test_region_row_restart_window(row_bin: Path, tmp_path: Path):
    r = subprocess.run([str(row_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: region row restart" in r.stdout
    log = r.stderr
    # the log lines the OLED plan reads
    assert "dw3-region: region_default ask: no restart prompt" in log
    assert "dw3-region: region_default us is the running region: no restart prompt" in log
    assert "dw3-region: restart prompt for eu (title)" in log
    assert "dw3-region: restart prompt for eu (in game)" in log
    assert "dw3-port-confirm: open restart in EU (title, cursor on No)" in log
    assert "dw3-port-confirm: open restart in EU (in game, cursor on No)" in log
    assert "dw3-region: relaunch declined; eu takes effect at the next start" in log
    assert "dw3-region: relaunch declined (cancelled); eu takes effect at the next start" in log
    assert "dw3-region: relaunch to eu requested" in log
    assert "dw3-region: saved last_region=eu region_default=eu before the relaunch" in log
    assert "dw3-region: relaunch to eu: leaving the game now (test host: exec)" in log
    assert "dw3-region: relaunch to eu failed: the saves could not be flushed; takes effect at the next start" in log
    assert "dw3-region: relaunch to eu failed: a save is being written; takes effect at the next start" in log
    assert "dw3-region: relaunch to eu failed: test host refused; takes effect at the next start" in log
    assert "dw3-region: relaunch to eu not available: no host (test); takes effect at the next start" in log
    assert "dw3-region: no restart prompt for eu: no data (test); takes effect at the next start" in log
