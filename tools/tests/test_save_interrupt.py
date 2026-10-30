"""P10.2: a card file on the SD is the old complete file or the new complete
file, never torn, whatever step the process dies at.

tools/tests/save_interrupt_main.c runs one process that dies (_exit, no atexit)
at a chosen step of the atomic write, and a second process that boots on what
is left. Fault points (port/memcard/memcard_host.c, memcard_host_test_fault_step):
1 tmp created and empty, 2 tmp half written, 3 tmp complete, 4 destination
removed, 5 renamed. nth 1 is the saveN.bin write, nth 2 the saveN.name sidecar.
Synthetic data only.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = [
    ROOT / "tools" / "tests" / "save_interrupt_main.c",
    ROOT / "port" / "memcard" / "memcard_host.c",
    ROOT / "port" / "memcard" / "save_convert.c",
    ROOT / "port" / "memcard" / "save_store.c",
]
OLD, NEW = 0xAA, 0xBB


@pytest.fixture(scope="module")
def drv(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("save_interrupt") / "save_interrupt"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-stringop-truncation",
           "-O1", "-I", str(ROOT / "port" / "memcard"), "-o", str(out), *map(str, SRC)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def run(drv: Path, *args: str) -> str:
    r = subprocess.run([str(drv), *args], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout.strip()


def kill_at(drv: Path, d: Path, step: int, nth: int = 1) -> None:
    run(drv, "write", str(d), str(step), str(nth))


# step -> what the next boot must find in saveN.bin
FLAT_EXPECT = {
    1: OLD,  # empty tmp, dest untouched: old, the stale tmp is dropped
    2: OLD,  # half-written tmp: old
    3: OLD,  # tmp complete but dest still there: old (the rename never ran)
    4: NEW,  # dest removed, tmp complete: the boot promotes the tmp
    5: NEW,  # finished
}


@pytest.mark.parametrize("step", sorted(FLAT_EXPECT))
def test_kill_during_flat_write(drv: Path, tmp_path: Path, step: int):
    kill_at(drv, tmp_path, step)
    assert run(drv, "boot", str(tmp_path)) == f"fill={FLAT_EXPECT[step]}"
    # never a torn or leftover file after the boot
    assert not (tmp_path / "save0.bin.tmp").exists()
    assert (tmp_path / "save0.bin").stat().st_size == 0x8000


@pytest.mark.parametrize("step", [1, 2, 3, 4, 5])
def test_kill_during_name_sidecar_write(drv: Path, tmp_path: Path, step: int):
    # the flat file was written whole, the sidecar write dies: the save is
    # the new one and the next boot still opens it
    kill_at(drv, tmp_path, step, nth=2)
    assert run(drv, "boot", str(tmp_path)) == f"fill={NEW}"
    assert not (tmp_path / "save0.name.tmp").exists()


def test_torn_tmp_with_missing_dest_is_not_promoted(drv: Path, tmp_path: Path):
    kill_at(drv, tmp_path, 5)
    (tmp_path / "save0.bin").unlink()
    (tmp_path / "save0.bin.tmp").write_bytes(b"\xcc" * 0x4000)  # half a file
    assert run(drv, "boot", str(tmp_path)) == "fill=none"
    assert not (tmp_path / "save0.bin.tmp").exists()


def test_stale_tmp_next_to_a_good_file_is_dropped(drv: Path, tmp_path: Path):
    kill_at(drv, tmp_path, 5)
    (tmp_path / "save0.bin.tmp").write_bytes(b"\xcc" * 0x8000)  # full size, old news
    assert run(drv, "boot", str(tmp_path)) == f"fill={NEW}"
    assert not (tmp_path / "save0.bin.tmp").exists()


def test_env_hook_arms_the_same_fault(drv: Path, tmp_path: Path):
    import os
    # write 3 is the second flush's saveN.bin write (setup: flat, sidecar)
    env = dict(os.environ, DW3_MEMCARD_FAULT_STEP="4", DW3_MEMCARD_FAULT_NTH="3")
    r = subprocess.run([str(drv), "write", str(tmp_path), "env", "1"], env=env,
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert not (tmp_path / "save0.bin").exists()  # killed after the remove
    assert (tmp_path / "save0.bin.tmp").stat().st_size == 0x8000
    assert run(drv, "boot", str(tmp_path)) == f"fill={NEW}"


# --- a game save that is half written when HOME / Quit arrives -----------------

def test_exit_waits_for_a_save_in_flight(drv: Path, tmp_path: Path):
    assert run(drv, "section", str(tmp_path), "wait") == f"ready=0 disk={OLD}"


def test_exit_gives_up_after_the_wait_and_keeps_the_old_file(drv: Path, tmp_path: Path):
    assert run(drv, "section", str(tmp_path), "timeout") == f"ready=1 disk={OLD}"


def test_exit_proceeds_once_the_save_section_closes(drv: Path, tmp_path: Path):
    assert run(drv, "section", str(tmp_path), "settle") == f"waited=1 ready=1 disk={NEW}"


def test_atexit_shutdown_flushes_a_finished_burst(drv: Path, tmp_path: Path):
    # the game loop returned right after its last write: the image is whole
    assert run(drv, "section", str(tmp_path), "shutdown") == f"disk={NEW}"


def test_atexit_shutdown_after_a_timeout_keeps_the_old_file(drv: Path, tmp_path: Path):
    assert run(drv, "section", str(tmp_path), "timeout-shutdown") == f"disk={OLD}"


# --- the log lines the OLED test plan (G2) greps for ----------------------------

def run_err(drv: Path, *args: str) -> str:
    r = subprocess.run([str(drv), *args], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stderr


def test_completed_write_logs_committed(drv: Path, tmp_path: Path):
    err = run_err(drv, "write", str(tmp_path), "5", "1")
    assert "dw3-save: write save0.bin committed (32768 bytes" in err
    assert err.count("save0.bin committed") == 2  # setup + the write under test


def test_kill_before_commit_logs_nothing_then_boot_keeps_previous_file(drv: Path, tmp_path: Path):
    # the second write dies at step 2: the first one is the previous file
    err = run_err(drv, "write", str(tmp_path), "2", "1")
    # only the setup write (the previous file) committed; the killed one did not
    assert err.count("save0.bin committed") == 1
    err = run_err(drv, "boot", str(tmp_path))
    assert "dw3-save: write save0.bin interrupted, previous file absent" in err \
        or "dw3-save: write save0.bin interrupted, previous file kept" in err


def test_boot_after_stale_tmp_logs_previous_file_kept(drv: Path, tmp_path: Path):
    kill_at(drv, tmp_path, 5)
    (tmp_path / "save0.bin.tmp").write_bytes(b"\xcc" * 0x4000)
    err = run_err(drv, "boot", str(tmp_path))
    assert "dw3-save: write save0.bin interrupted, previous file kept" in err


def test_boot_after_kill_between_remove_and_rename_logs_recovery(drv: Path, tmp_path: Path):
    kill_at(drv, tmp_path, 4)
    err = run_err(drv, "boot", str(tmp_path))
    assert "dw3-save: write save0.bin interrupted after the old file was removed" in err


def test_exit_timeout_logs_previous_file_kept(drv: Path, tmp_path: Path):
    err = run_err(drv, "section", str(tmp_path), "timeout")
    assert "interrupted, previous file kept (exit with a save section open)" in err


def test_hooks_are_wired():
    lib = (ROOT / "port" / "memcard" / "memcard_libmcrd.c").read_text()
    assert "memcard_host_section_begin(slot)" in lib
    boot = (ROOT / "platforms" / "switch" / "source" / "boot_main.c").read_text()
    assert "memcard_host_exit_ready(" in boot
    assert "memcard_host_flush_settled()" in boot
    restart = (ROOT / "port" / "options" / "restart.c").read_text()
    assert "memcard_host_section_open_any()" in restart
