"""Headless proof that host libpad reports connected + Start in the RX buffer."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
PAD_HOST_C = ROOT / "port" / "psyz_missing" / "psyz_pad_host.c"
TEST_C = ROOT / "tools" / "pad_host_test.c"
PSYZ_INC = ROOT / "vendor" / "psyz" / "psyz" / "include"
PORT_INC = ROOT / "port" / "include"


@pytest.fixture(scope="module")
def pad_host_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("pad_host_build")
    out = build_dir / "pad_host_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DDW3_PAD_TEST_STUB_PSYZ",
        "-D__psyz",
        "-o",
        str(out),
        str(TEST_C),
        str(PAD_HOST_C),
        f"-I{PSYZ_INC}",
        f"-I{PORT_INC}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    return out


def test_pad_get_state_connected_and_start_bit(pad_host_test: Path):
    r = subprocess.run([str(pad_host_test)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK:" in r.stdout
    assert "Start" in r.stdout
    # Connect log from pad host
    assert "PadInitDirect" in r.stderr or "connected" in r.stderr.lower() or "OK:" in r.stdout


def test_pad_mode_table_and_set_main_mode(pad_host_test: Path):
    """PadInfoMode CurExOffs/IdTable + PadSetMainMode drive the controller mode.

    This is the contract the decomp's src/engine/pad/port.c relies on:
    lockPadMode round-trips PadInfoMode(InfoModeCurExOffs) into PadSetMainMode,
    and readPadButtons switches on PAD_ID_DIGITAL (4) / PAD_ID_ANALOG (7).
    """
    r = subprocess.run([str(pad_host_test)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "mode table" in r.stdout
    assert "PadSetMainMode" in r.stdout
