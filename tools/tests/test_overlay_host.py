"""P2.6 host test: static overlay restore vs preserve semantics."""

from __future__ import annotations

import os
import signal
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
HOST_C = ROOT / "port" / "overlays" / "overlay_host.c"
HOST_H = ROOT / "port" / "overlays" / "overlay_host.h"
TEST_C = ROOT / "tools" / "overlay_host_test.c"


@pytest.fixture(scope="module")
def overlay_host_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("overlay_build")
    out = build_dir / "overlay_host_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-DPORT_HOST=1",
        "-o",
        str(out),
        str(TEST_C),
        str(HOST_C),
        f"-I{ROOT / 'port' / 'overlays'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    assert HOST_H.is_file()
    return out


def _env(**extra):
    env = {k: v for k, v in os.environ.items()
           if k not in ("DW3_OVERLAY_STRICT", "DW3_REPLAY")}
    env.update(extra)
    return env


def test_reset_vs_preserve(overlay_host_test: Path):
    r = subprocess.run(
        [str(overlay_host_test)],
        capture_output=True,
        text=True,
        check=False,
        env=_env(),
    )
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: overlay reset vs preserve" in r.stdout
    assert "OK: overlay names" in r.stdout
    # The unknown-id cases keep the stable sweep marker and add an error line.
    assert "dw3: overlay restore file=0x1fb kind=unknown UNKNOWN" in r.stderr
    assert "dw3: ERROR overlay restore failed: file=0x1fb (507)" in r.stderr
    assert "FATAL" not in r.stderr


@pytest.mark.parametrize("extra", [
    {"DW3_OVERLAY_STRICT": "1"},
    {"DW3_REPLAY": "/nonexistent/scenario.rec"},  # replay runs are strict
])
def test_unknown_restore_aborts_in_strict_mode(overlay_host_test: Path, extra):
    r = subprocess.run([str(overlay_host_test), "strict"], capture_output=True,
                       text=True, check=False, env=_env(**extra))
    assert r.returncode == -signal.SIGABRT, r.stderr + r.stdout
    assert "kind=unknown UNKNOWN" in r.stderr
    assert "dw3: FATAL overlay restore file=0x1fb failed in strict mode" in r.stderr


def test_strict_env_can_be_turned_off(overlay_host_test: Path):
    r = subprocess.run([str(overlay_host_test), "strict"], capture_output=True,
                       text=True, check=False,
                       env=_env(DW3_REPLAY="/nonexistent/scenario.rec",
                                DW3_OVERLAY_STRICT="0"))
    assert r.returncode == 1, r.stderr + r.stdout
    assert "strict mode not enabled" in r.stderr
