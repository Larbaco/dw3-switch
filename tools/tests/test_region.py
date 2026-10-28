"""P6.24 slice 1: the run-time region choice (port/region/dw3_region.c) and the
PC launcher (platforms/pc/src/launcher_main.c) with stand-in game binaries, so
no game data is needed."""

from __future__ import annotations

import os
import stat
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
REGION = ROOT / "port" / "region"
OPTIONS = ROOT / "port" / "options"
INCLUDE = ROOT / "port" / "include"


def build(out: Path, *srcs: Path) -> Path:
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
           *map(str, srcs), f"-I{REGION}", f"-I{INCLUDE}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def region_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("region") / "region_test", TESTS / "region_main.c",
                 REGION / "dw3_region.c", OPTIONS / "options_cfg.c")


@pytest.fixture(scope="module")
def launcher(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("launcher") / "dw3-pc",
                 ROOT / "platforms" / "pc" / "src" / "launcher_main.c",
                 REGION / "dw3_region.c", OPTIONS / "options_cfg.c")


@pytest.fixture(scope="module")
def launcher_bin(tmp_path_factory) -> Path:
    """P6.24 slice 2: the pure helpers of the Switch launcher NRO."""
    return build(tmp_path_factory.mktemp("nxlauncher") / "launcher_test", TESTS / "launcher_main.c",
                 REGION / "dw3_launcher.c", REGION / "dw3_region.c", OPTIONS / "options_cfg.c")


def test_switch_launcher_helpers(launcher_bin: Path, tmp_path: Path):
    r = subprocess.run([str(launcher_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: launcher" in r.stdout


def test_region_logic(region_bin: Path, tmp_path: Path):
    r = subprocess.run([str(region_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: region" in r.stdout


# ---- launcher ----

def make_base(base: Path, us: bool, eu: bool) -> None:
    for present, v, exe in ((us, "us", "SLUS_014.36"), (eu, "eu", "SLES_039.36")):
        if present:
            d = base / v
            d.mkdir(parents=True)
            (d / f"files_{v}.json").write_text("{}")
            (d / exe).write_text("x")


@pytest.fixture
def fake_bins(tmp_path: Path) -> dict[str, str]:
    """Stand-ins for dw3-pc-us / dw3-pc-eu: print who they are and what the
    launcher handed them, then exit with a status that proves they ran."""
    out = {}
    for v, rc in (("us", 11), ("eu", 12)):
        p = tmp_path / f"fake-{v}"
        p.write_text(f'#!/bin/sh\necho "started-{v} runtime=$DW3_RUNTIME args=$*"\nexit {rc}\n')
        p.chmod(p.stat().st_mode | stat.S_IXUSR)
        out[v] = str(p)
    return out


def run(launcher: Path, base: Path, fake_bins, *args, env=None, cfg: Path | None = None):
    e = {k: v for k, v in os.environ.items()
         if not k.startswith("DW3_")}
    e.update(DW3_RUNTIME_BASE=str(base), DW3_PC_BIN_US=fake_bins["us"],
             DW3_PC_BIN_EU=fake_bins["eu"], DW3_CONFIG_DIR=str(cfg or base))
    e.update(env or {})
    return subprocess.run([str(launcher), *args], capture_output=True, text=True, env=e,
                          stdin=subprocess.DEVNULL, timeout=60)


def test_one_region_boots_without_picker(launcher, fake_bins, tmp_path):
    for us, eu, want, rc in ((True, False, "us", 11), (False, True, "eu", 12)):
        base = tmp_path / want
        make_base(base, us, eu)
        r = run(launcher, base, fake_bins, "--foo")
        assert r.returncode == rc, r.stderr
        assert f"started-{want} runtime={base}/{want} args=--foo" in r.stdout
        assert "picker chose" not in r.stderr and "-> only" in r.stderr
        assert (base / "port_options.cfg").read_text().count(f"last_region={want}") == 1


def test_no_data_shows_setup_message(launcher, fake_bins, tmp_path):
    base = tmp_path / "empty"
    base.mkdir()
    r = run(launcher, base, fake_bins)
    assert r.returncode == 2
    assert "no game data found" in r.stderr and "SLUS_014.36" in r.stderr and "SLES_039.36" in r.stderr
    assert not (base / "port_options.cfg").exists()


def test_both_regions_picker_and_memory(launcher, fake_bins, tmp_path):
    base = tmp_path / "both"
    make_base(base, True, True)
    # No terminal, no scripted keys: starts the default (US) at once, no hang.
    r = run(launcher, base, fake_bins)
    assert r.returncode == 11 and "no terminal for the picker" in r.stderr
    # Scripted keys: right (EU), then A.
    r = run(launcher, base, fake_bins, env={"DW3_REGION_PICK_KEYS": "ra"})
    assert r.returncode == 12 and "picker chose eu" in r.stderr
    # Last region is the next default; the countdown (30 idle steps) boots it.
    r = run(launcher, base, fake_bins, env={"DW3_REGION_PICK_KEYS": ""})
    assert r.returncode == 12 and "picker chose eu (countdown)" in r.stderr
    # A key press cancels the countdown; without A it would wait, so end with A.
    r = run(launcher, base, fake_bins, env={"DW3_REGION_PICK_KEYS": "l...................................a"})
    assert r.returncode == 11 and "(countdown)" not in r.stderr
    # Y = always: stored, and the picker is skipped next time.
    r = run(launcher, base, fake_bins, env={"DW3_REGION_PICK_KEYS": "rya"})
    assert r.returncode == 12 and "(always)" in r.stderr
    assert "region_default=eu" in (base / "port_options.cfg").read_text()
    r = run(launcher, base, fake_bins)
    assert r.returncode == 12 and "-> region_default" in r.stderr and "picker chose" not in r.stderr
    # --pick (the Minus stand-in) brings the picker back; q quits cleanly.
    r = run(launcher, base, fake_bins, "--pick", env={"DW3_REGION_PICK_KEYS": "q"})
    assert r.returncode == 0 and "started-" not in r.stdout and "quit from the picker" in r.stderr
    # Back to ask by editing the file (the Options row is a later slice);
    # last_region is gone with it, so the default entry is US.
    (base / "port_options.cfg").write_text("version=1\nregion_default=ask\nend=1\n")
    r = run(launcher, base, fake_bins, env={"DW3_REGION_PICK_KEYS": "a"})
    assert r.returncode == 11 and "picker chose us" in r.stderr


def test_forced_region_skips_picker_and_files(launcher, fake_bins, tmp_path):
    base = tmp_path / "both"
    make_base(base, True, True)
    cfg = tmp_path / "cfg"
    cfg.mkdir()
    for args, env, want, rc in ((("--region", "eu"), {}, "eu", 12),
                                ((), {"DW3_REGION": "US"}, "us", 11)):
        r = run(launcher, base, fake_bins, *args, env=env, cfg=cfg)
        assert r.returncode == rc and f"started-{want}" in r.stdout
        assert "picker chose" not in r.stderr
    assert list(cfg.iterdir()) == []  # a forced region writes nothing
    # DW3_RUNTIME already set (the replay runner): kept as is.
    r = run(launcher, base, fake_bins, "--region", "us", env={"DW3_RUNTIME": "/some/where"}, cfg=cfg)
    assert "runtime=/some/where" in r.stdout
    # Forced region with no data and no DW3_RUNTIME: the setup message.
    one = tmp_path / "one"
    make_base(one, True, False)
    r = run(launcher, one, fake_bins, "--region", "eu")
    assert r.returncode == 2 and "no eu data" in r.stderr
    r = run(launcher, one, fake_bins, "--region", "xx")
    assert r.returncode == 2


@pytest.fixture(scope="module")
def region_rt_bin(tmp_path_factory) -> Path:
    """P6.24: the run-time choice of a `both` image (port/region/dw3_region_rt.c)."""
    return build(tmp_path_factory.mktemp("regionrt") / "region_rt_test", TESTS / "region_rt_main.c",
                 REGION / "dw3_region_rt.c", REGION / "dw3_region.c", OPTIONS / "options_cfg.c")


def test_region_resolve(region_rt_bin: Path, tmp_path: Path):
    r = subprocess.run([str(region_rt_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: region_rt" in r.stdout
