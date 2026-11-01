"""D3 / P6.26: one global options file. The migration of the old per-region files
(port/options/options_cfg.c, dw3_optcfg_migrate_global), the folder rule, and the
registry that loads the result (port/options/port_options.c).

Owner decisions of 2026-10-31 (a1 b2 c1 d2): one file, nothing per region, the
last_region file wins a conflict, the old files are deleted only after the new one
is written and read back, with a verified .bak copy first."""

from __future__ import annotations

import os
import stat
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
OPTIONS = ROOT / "port" / "options"
CFG = "port_options.cfg"

DONE, FAILED = 10, 11


def build(out: Path, *sources: Path, defs=()) -> Path:
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", *defs, "-o", str(out),
           *map(str, sources), f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def mig(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("optglobal") / "options_global_test",
                 TESTS / "options_global_main.c", OPTIONS / "options_cfg.c")


def run(mig: Path, *args):
    r = subprocess.run([str(mig), *map(str, args)], capture_output=True, text=True)
    return r.returncode, r.stdout.splitlines(), r.stderr


def put(path: Path, **kv):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["version=1"] + [f"{k}={v}" for k, v in kv.items()] + ["end=1"]
    path.write_text("\n".join(lines) + "\n")


def keys_of(path: Path) -> dict:
    return dict(l.split("=", 1) for l in path.read_text().splitlines()
                if "=" in l and not l.startswith("#"))


# --- the folder rule ---------------------------------------------------------------------------

@pytest.mark.parametrize("cfgdir,root,want", [
    ("-", "sdmc:/switch/dw3/us", "sdmc:/switch/dw3"),
    ("-", "sdmc:/switch/dw3/eu/", "sdmc:/switch/dw3"),
    ("-", "runtime/us", "runtime"),
    ("-", "/x/y/runtime/EU", "/x/y/runtime/EU"),      # only lower-case us / eu folders
    ("-", "/mnt/disc/focus", "/mnt/disc/focus"),       # a hand-made runtime is its own folder
    ("-", "/mnt/disc/bus", "/mnt/disc/bus"),
    ("-", "us", "us"),
    ("-", "/us", "/"),
    ("/cfg", "sdmc:/switch/dw3/us", "/cfg"),            # DW3_CONFIG_DIR wins
])
def test_global_dir_rule(mig, cfgdir, root, want):
    rc, out, _ = run(mig, "dir", cfgdir, root)
    assert out == [f"0 {want}"], out


def test_global_dir_too_small_buffer(mig):
    rc, out, _ = run(mig, "dir", "-", "x" * 80 + "/us")
    assert out[0].startswith("-1")


# --- the migration -----------------------------------------------------------------------------

def tree(tmp_path: Path, us=None, eu=None, top=None) -> Path:
    g = tmp_path / "dw3"
    g.mkdir()
    if us is not None:
        put(g / "us" / CFG, **us)
    if eu is not None:
        put(g / "eu" / CFG, **eu)
    if top is not None:
        put(g / CFG, **top)
    return g


def test_conflict_last_region_wins_and_every_conflict_is_logged(mig, tmp_path):
    g = tree(tmp_path,
             us=dict(widescreen=0, music_volume=7, save_store=1, button_layout="playstation", defaults_gen=2),
             eu=dict(widescreen=1, music_volume=7, text_speed=2, button_layout="nintendo", defaults_gen=2),
             top=dict(region_default="us", last_region="eu"))
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    kv = keys_of(g / CFG)
    assert kv["widescreen"] == "1" and kv["button_layout"] == "nintendo"   # the EU file won
    assert kv["music_volume"] == "7"                                        # equal: no conflict
    assert kv["save_store"] == "1" and kv["text_speed"] == "2"              # one file only
    assert kv["region_default"] == "us" and kv["last_region"] == "eu"       # folded, untouched
    assert kv["options_global"] == "1" and kv["defaults_gen"] == "2"
    assert "dw3-options: migrate winner=eu (last_region=eu)" in out
    assert "dw3-options: migrate key=widescreen us=0 eu=1 kept=1" in out
    assert "dw3-options: migrate key=button_layout us=playstation eu=nintendo kept=nintendo" in out
    assert not [l for l in out if "key=music_volume" in l or "key=save_store" in l]
    n = len(kv) - 2                                      # without version and end
    assert (f"dw3-options: global file {g}/{CFG} (migrated from us+eu, {n} keys, 2 conflicts: "
            "widescreen, button_layout)") in out
    # the old files: backed up, then deleted, in this order
    assert not (g / "us" / CFG).exists() and not (g / "eu" / CFG).exists()
    assert (g / f"{CFG}.us.bak").read_text().count("save_store=1") == 1
    assert "text_speed=2" in (g / f"{CFG}.eu.bak").read_text()
    i_glob = next(i for i, l in enumerate(out) if "migrated from us+eu" in l)
    i_bak = next(i for i, l in enumerate(out) if l.startswith("dw3-options: backup us/"))
    i_del = next(i for i, l in enumerate(out) if l == "dw3-options: deleted us/port_options.cfg after read-back ok")
    assert i_glob < i_bak < i_del
    assert "dw3-options: deleted eu/port_options.cfg after read-back ok" in out
    assert not [l for l in out if l.startswith("W:")]


def test_missing_last_region_means_us_wins(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0), eu=dict(widescreen=1))
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    assert keys_of(g / CFG)["widescreen"] == "0"
    assert "dw3-options: migrate winner=us (last_region=missing)" in out
    assert "dw3-options: migrate key=widescreen us=0 eu=1 kept=0" in out
    # an invalid value falls back the same way
    g2 = tree(tmp_path / "b" if (tmp_path / "b").mkdir() is None else tmp_path,
              us=dict(widescreen=0), eu=dict(widescreen=1), top=dict(last_region="mars"))
    rc, out, _ = run(mig, "migrate", g2)
    assert keys_of(g2 / CFG)["widescreen"] == "0"


def test_a_single_file_is_taken_whole(mig, tmp_path):
    g = tree(tmp_path, eu=dict(widescreen=0, save_store=1, stage_sweep=2))
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    kv = keys_of(g / CFG)
    assert (kv["widescreen"], kv["save_store"], kv["stage_sweep"], kv["options_global"]) == ("0", "1", "2", "1")
    assert any("migrated from eu, " in l and ", 0 conflicts)" in l for l in out)
    assert not (g / "eu" / CFG).exists() and (g / f"{CFG}.eu.bak").exists()
    assert not (g / f"{CFG}.us.bak").exists()


def test_a_fresh_install_has_nothing_to_migrate(mig, tmp_path):
    g = tmp_path / "dw3"
    g.mkdir()
    rc, out, _ = run(mig, "migrate", g)
    assert rc == 0
    assert out == [f"dw3-options: global file {g}/{CFG} (no us/eu port_options.cfg to migrate)"]
    assert list(g.iterdir()) == []                      # nothing created
    # a top-level file that only holds the region keys is already the global file
    put(g / CFG, region_default="eu", last_region="eu")
    rc, out, _ = run(mig, "migrate", g)
    assert rc == 0 and {k: v for k, v in keys_of(g / CFG).items() if k not in ("version", "end")} == {"region_default": "eu", "last_region": "eu"}


def test_the_marker_makes_it_one_shot(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0), top=dict(last_region="us"))
    assert run(mig, "migrate", g)[0] == DONE
    # an old build (or a restored backup) puts an old file back: it is ignored and kept
    put(g / "us" / CFG, widescreen=1, save_store=1)
    rc, out, _ = run(mig, "migrate", g)
    assert rc == 0 and out == []
    assert (g / "us" / CFG).exists() and keys_of(g / CFG)["widescreen"] == "0"


def test_failed_write_deletes_nothing_and_retries_next_start(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0, save_store=1), eu=dict(widescreen=1),
             top=dict(last_region="us"))
    before = (g / CFG).read_text()
    (g / f"{CFG}.tmp").mkdir()                          # the write cannot create its temp file
    rc, out, _ = run(mig, "migrate", g)
    assert rc == FAILED
    assert any(l.startswith("W: dw3-options: migration NOT done (cannot write ") and
               l.endswith("), old files kept") for l in out)
    assert (g / "us" / CFG).exists() and (g / "eu" / CFG).exists()
    assert not list(g.glob("*.bak*"))
    assert (g / CFG).read_text() == before              # the previous global file is intact
    (g / f"{CFG}.tmp").rmdir()                          # the next start works
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE and not (g / "us" / CFG).exists()


def test_an_unreadable_old_file_blocks_the_migration(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0), eu=dict(widescreen=1))
    big = g / "eu" / CFG
    big.write_text("x=" + "1" * 5000 + "\n")           # larger than the limit: not ours
    rc, out, _ = run(mig, "migrate", g)
    assert rc == FAILED
    assert "W: dw3-options: migration NOT done (cannot read eu/port_options.cfg), old files kept" in out
    assert (g / "us" / CFG).exists() and big.exists() and not (g / CFG).exists()


def test_too_many_keys_fails_before_writing(mig, tmp_path):
    us = {f"a{i}": i for i in range(30)}
    eu = {f"b{i}": i for i in range(30)}
    g = tree(tmp_path, us=us, eu=eu)
    rc, out, _ = run(mig, "migrate", g)
    assert rc == FAILED
    assert any("migration NOT done (too many keys" in l for l in out)
    assert (g / "us" / CFG).exists() and (g / "eu" / CFG).exists() and not (g / CFG).exists()


def test_an_existing_different_backup_is_never_overwritten(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0))
    (g / f"{CFG}.us.bak").write_text("precious\n")
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    assert (g / f"{CFG}.us.bak").read_text() == "precious\n"
    assert "widescreen=0" in (g / f"{CFG}.us.bak.1").read_text()
    assert not (g / "us" / CFG).exists()


def test_backup_is_verified_byte_for_byte(mig, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0, music_volume=3))
    original = (g / "us" / CFG).read_bytes()
    assert run(mig, "migrate", g)[0] == DONE
    assert (g / f"{CFG}.us.bak").read_bytes() == original


def test_defaults_gen_takes_the_older_generation(mig, tmp_path):
    g = tree(tmp_path, us=dict(defaults_gen=2, button_layout="nintendo"),
             eu=dict(button_layout="playstation"), top=dict(last_region="us"))
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    kv = keys_of(g / CFG)
    assert "defaults_gen" not in kv                      # counts as 1: the D2 migration still runs
    assert any("key=defaults_gen us=2 eu=(none) kept=1" in l for l in out)
    # both on 2: stays 2, no conflict line
    g2 = tmp_path / "two"
    g2.mkdir()
    put(g2 / "us" / CFG, defaults_gen=2)
    put(g2 / "eu" / CFG, defaults_gen=2)
    rc, out, _ = run(mig, "migrate", g2)
    assert keys_of(g2 / CFG)["defaults_gen"] == "2" and not [l for l in out if "key=defaults_gen" in l]


def test_a_stray_key_in_the_old_top_level_file_gives_way(mig, tmp_path):
    # stage_sweep=1 was put in the picker's file by mistake: the game never read it
    g = tree(tmp_path, us=dict(stage_sweep=0), eu=dict(stage_sweep=0),
             top=dict(region_default="eu", last_region="us", stage_sweep=1))
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE
    kv = keys_of(g / CFG)
    assert kv["stage_sweep"] == "0" and kv["region_default"] == "eu"
    assert "dw3-options: migrate key=stage_sweep top=1 regions=0 kept=0" in out


def test_region_keys_in_a_region_file_do_not_override_the_global_ones(mig, tmp_path):
    g = tree(tmp_path, us=dict(region_default="us", last_region="us"),
             eu=dict(widescreen=1), top=dict(region_default="eu", last_region="eu"))
    rc, out, _ = run(mig, "migrate", g)
    kv = keys_of(g / CFG)
    assert (kv["region_default"], kv["last_region"]) == ("eu", "eu")


def test_a_recovered_tmp_file_is_migrated_and_deleted(mig, tmp_path):
    g = tree(tmp_path)
    (g / "eu").mkdir()
    (g / "eu" / f"{CFG}.tmp").write_text("version=1\nwidescreen=0\nend=1\n")   # a write died mid-way
    rc, out, _ = run(mig, "migrate", g)
    assert rc == DONE and keys_of(g / CFG)["widescreen"] == "0"
    assert not (g / "eu" / f"{CFG}.tmp").exists()


# --- the registry on top of the migration -------------------------------------------------------

@pytest.fixture(scope="module")
def reg(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("optreg") / "options_persist_test",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c", OPTIONS / "button_map.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST"])


def session(binary: Path, cfgdir: Path, *steps: str):
    r = subprocess.run([str(binary), str(cfgdir), *steps], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip().splitlines(), r.stderr


def test_the_game_boots_on_the_merged_values_and_logs_the_migration(reg, tmp_path):
    g = tree(tmp_path,
             us=dict(widescreen=0, music_volume=3, save_store=1, defaults_gen=2, button_layout="playstation"),
             eu=dict(widescreen=1, music_volume=8, defaults_gen=2, button_layout="nintendo"),
             top=dict(region_default="ask", last_region="us"))
    out, err = session(reg, g, "volumes", "fileval", "save_store", "layout")
    assert out[0] == "music_volume=3 sfx_volume=10"            # last_region = us wins
    assert out[1] == "save_store=1" and out[2] == "button_layout=0"
    assert "migrated from us+eu" in err and "migration NOT done" not in err
    assert "dw3-options: deleted us/port_options.cfg after read-back ok" in err
    # the next start loads the global file only and does not migrate again
    out, err = session(reg, g, "volumes")
    assert out == ["music_volume=3 sfx_volume=10"] and "migrate" not in err


def test_a_failed_migration_keeps_the_old_files_and_never_stamps_the_marker(reg, tmp_path):
    g = tree(tmp_path, us=dict(widescreen=0, save_store=1), eu=dict(widescreen=1))
    (g / "eu" / CFG).write_text("x=" + "1" * 5000 + "\n")           # cannot be read: blocks it
    out, err = session(reg, g, "open", "row", "3", "press", "close")   # a change saves the file
    assert "migration NOT done (cannot read eu/port_options.cfg), old files kept" in err
    assert (g / "us" / CFG).exists() and (g / "eu" / CFG).exists()
    assert "options_global" not in keys_of(g / CFG)                  # so the next start retries
    (g / "eu" / CFG).write_text("version=1\nwidescreen=1\nend=1\n")
    out, err = session(reg, g, "volumes")
    assert "migrated from us+eu" in err and not (g / "us" / CFG).exists()


def test_a_save_writes_the_marker_on_a_fresh_install(reg, tmp_path):
    g = tmp_path / "dw3"
    g.mkdir()
    session(reg, g, "open", "row", "3", "press", "close")
    kv = keys_of(g / CFG)
    assert kv["options_global"] == "1" and kv["defaults_gen"] == "2"
    assert not list(g.glob("*.bak*"))


def test_the_old_top_level_keys_survive_the_migration_and_later_saves(reg, tmp_path):
    g = tree(tmp_path, eu=dict(widescreen=0), top=dict(region_default="eu", last_region="eu"))
    session(reg, g, "open", "row", "3", "press", "close")
    kv = keys_of(g / CFG)
    assert (kv["region_default"], kv["last_region"], kv["widescreen"]) == ("eu", "eu", "0")
