"""P5.5 stage sweep (port/sweep/stage_sweep.c): the state machine against a
fake game, the progress file and the resume after a crash, and the stage
table against the decomp. docs/P5.5_STAGE_SWEEP.md."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
SWEEP = ROOT / "port" / "sweep"


@pytest.fixture(scope="module")
def sweep_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("sweep") / "stage_sweep_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_SWEEP_TEST", "-pthread", "-o", str(out), str(TESTS / "stage_sweep_main.c"),
           str(SWEEP / "stage_sweep.c"), f"-I{ROOT / 'port' / 'include'}",
           f"-I{SWEEP}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def num(mode: int) -> str:
    """WSTAG number of a game mode, from the generated table."""
    for m, n in re.findall(r"\{ 0x([0-9A-F]+), (\d+) \}", (SWEEP / "stage_list.inc").read_text()):
        if int(m, 16) == mode:
            return f"WSTAG{int(n):03d}"
    raise KeyError(mode)


def run(binary: Path, d: Path, scen: str, mode: str = "", want: int = 0):
    env = {k: v for k, v in os.environ.items() if not k.startswith("DW3_")}
    args = [str(binary), str(d), scen] + ([mode] if mode else [])
    r = subprocess.run(args, capture_output=True, text=True, env=env, timeout=120)
    assert r.returncode == want, r.stderr
    return r.stderr


def test_table_matches_decomp():
    if not (ROOT / "vendor/dw3_decomp/src/field/field_mode/data/fieldstg.c").exists():
        pytest.skip("decomp submodule not checked out")
    r = subprocess.run([sys.executable, str(ROOT / "tools/gen_sweep_stages.py"),
                        "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_all_stages_ok(sweep_bin, tmp_path):
    err = run(sweep_bin, tmp_path, "ok")
    assert "dw3-sweep: stage WSTAG200 ok in " in err
    assert "(frames 30, mem free 12345, load " in err
    assert "dw3-sweep: done 238/238 ok, failed: none" in err
    assert (tmp_path / "stage_sweep.txt").read_text().rstrip().endswith("done")


def test_finished_sweep_is_not_run_again(sweep_bin, tmp_path):
    run(sweep_bin, tmp_path, "ok")
    err = run(sweep_bin, tmp_path, "ok")
    assert "the previous sweep is complete; not running again" in err
    assert "previous sweep, done 238/238 ok" in err
    assert "begin" not in err


def test_stuck_stage_times_out(sweep_bin, tmp_path):
    err = run(sweep_bin, tmp_path, "stuck", "0x206")
    assert f"stage {num(0x206)} FAILED: timeout: mode 0x206 not reached in 20 s" in err
    assert f"done 237/238 ok, failed: {num(0x206)} (timeout" in err


def test_stage_that_does_not_return(sweep_bin, tmp_path):
    # the fake stage ignores every request for the select, so the sweep
    # cannot go on after the failure (exit 1 = aborted)
    err = run(sweep_bin, tmp_path, "noreturn", "0x209", want=1)
    assert f"stage {num(0x209)} FAILED: did not return to the stage select" in err
    assert "ABORTED: the stage select (mode 0x1500) was not reached" in err
    assert "not run:" in err


def test_stage_that_leaves_by_itself(sweep_bin, tmp_path):
    err = run(sweep_bin, tmp_path, "leave", "0x21d")
    assert re.search(r"FAILED: left the stage for mode 0x1500 after \d+ frames", err)


def test_crash_is_reported_and_the_run_resumes(sweep_bin, tmp_path):
    run(sweep_bin, tmp_path, "crash", "0x209", want=77)
    prog = (tmp_path / "stage_sweep.txt").read_text()
    assert prog.rstrip().endswith("begin 0x209")
    err = run(sweep_bin, tmp_path, "ok")
    assert f"stage {num(0x209)} FAILED: crash or hang in the previous run" in err
    assert "resuming:" in err
    assert f"stage {num(0x200)} ok" not in err   # done before the crash
    assert f"done 237/238 ok, failed: {num(0x209)} (crash or hang" in err


def test_hang_is_caught_by_the_watchdog_and_resumes(sweep_bin, tmp_path):
    err = run(sweep_bin, tmp_path, "hang", "0x209")        # the watchdog exits 0
    assert f"stage {num(0x209)} FAILED: hang: no game frame for 1 s" in err
    assert "start the game again to continue" in err
    prog = (tmp_path / "stage_sweep.txt").read_text()
    assert f"fail 0x209 hang" in prog
    err = run(sweep_bin, tmp_path, "ok")
    assert "crash or hang in the previous run" not in err   # already recorded
    assert f"done 237/238 ok, failed: {num(0x209)} (hang" in err


def test_only_limits_the_set(sweep_bin, tmp_path):
    err = run(sweep_bin, tmp_path, "only", "0x206,0x209")
    assert "done 2/2 ok, failed: none" in err
    assert err.count("ok in ") == 2


def test_off_by_default(sweep_bin, tmp_path):
    # no DW3_STAGE_SWEEP in the environment: the fake game would loop to its
    # limit and return 3 without a line
    env = {k: v for k, v in os.environ.items() if not k.startswith("DW3_")}
    src = tmp_path / "off.c"
    src.write_text(
        '#include "port_stage_sweep.h"\n'
        "int main(void){ int i; for(i=0;i<1000;i++) if(dw3_sweep_tick(0xE00)) "
        "return 1; return dw3_sweep_enabled(); }\n")
    exe = tmp_path / "off"
    r = subprocess.run(["gcc", "-std=gnu11", "-DDW3_SWEEP_TEST", "-pthread", "-o", str(exe),
                        str(src), str(SWEEP / "stage_sweep.c"),
                        f"-I{ROOT / 'port' / 'include'}", f"-I{SWEEP}"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    r = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    assert r.returncode == 0 and r.stderr == ""


def test_stage_sweep_in_the_top_level_file_is_logged_and_ignored(tmp_path):
    """P51 item 7: the picker's file holds stage_sweep=1 (the wrong file): one
    log line names both files, the sweep stays off."""
    top = tmp_path / "dw3"
    region = top / "eu"
    region.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("DW3_")}
    env["DW3_STAGE_SWEEP_DIR"] = str(region)
    src = tmp_path / "stray.c"
    src.write_text(
        '#include "port_stage_sweep.h"\n'
        "int main(void){ int i; for(i=0;i<1000;i++) if(dw3_sweep_tick(0xE00)) "
        "return 1; return dw3_sweep_enabled(); }\n")
    exe = tmp_path / "stray"
    r = subprocess.run(["gcc", "-std=gnu11", "-DDW3_SWEEP_TEST", "-pthread", "-o", str(exe),
                        str(src), str(SWEEP / "stage_sweep.c"),
                        f"-I{ROOT / 'port' / 'include'}", f"-I{SWEEP}"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    (top / "port_options.cfg").write_text("region_default=eu\nstage_sweep=1\nend=1\n")
    r = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    assert r.returncode == 0                 # the sweep stays off
    assert r.stderr.count("dw3-sweep: stage_sweep found in") == 1
    assert (f"dw3-sweep: stage_sweep found in {top}/port_options.cfg, ignored; "
            f"put it in {region}/port_options.cfg") in r.stderr
    # no stray key, no line
    (top / "port_options.cfg").write_text("region_default=eu\nstage_sweep=0\nend=1\n")
    r = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    assert r.returncode == 0 and r.stderr == ""
