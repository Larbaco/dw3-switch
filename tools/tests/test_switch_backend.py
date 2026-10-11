"""P4.5 regression tests for the Switch backend, runnable on the host.

`nx_file.c` / `nx_os.c` normally link only against libnx (devkitA64), which this
host does not have. `tools/testdata/switch_stub/` provides a minimal libnx
stand-in, so the *real* units are compiled and driven here: the assertions are on
observed behaviour, not on source shape.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
STUB = ROOT / "tools" / "testdata" / "switch_stub"
BACKEND = ROOT / "platforms" / "switch" / "psyz_backend"


@pytest.fixture(scope="module")
def results(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("switch_stub_build") / "harness"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(STUB / "harness.c"),
        str(STUB / "stub.c"),
        str(BACKEND / "nx_file.c"),
        str(BACKEND / "nx_os.c"),
        f"-I{STUB}",
        f"-I{BACKEND}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        if "=" in line:
            key, _, value = line.partition("=")
            parsed[key.strip()] = value.strip()
    return parsed


def test_mount_succeeds_when_sdmc_already_mounted(results):
    """fsdevMountSdmc() failure + an existing sdmc: device is not a failure.

    Pre-P4.5 this returned false, so a run where hbmenu (or a re-entry) had
    already mounted sdmc: logged "runtime paths will not resolve" and gave up,
    although the paths resolved.
    """
    assert results.get("fs_mount_already_mounted") == "1"


def test_mount_fails_only_when_device_missing(results):
    assert results.get("fs_mount_genuine_failure") == "0"
    assert results.get("fs_mount_success") == "1"


def test_thread_start_failure_closes_live_handle(results):
    """threadCreate() succeeded, so the handle must be threadClose()d.

    Pre-P4.5 the failure path free()'d the Thread storage with the handle still
    live: stub handle stays live and threadClose_count == 0, both asserted here.
    """
    assert results.get("thread_start_rc") == "-1"
    assert results.get("thread_create_count") == "1"
    assert results.get("thread_close_count") == "1"
    assert results.get("thread_handle_live") == "0"
