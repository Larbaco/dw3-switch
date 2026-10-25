"""P6.20 Cheats screen (port/options/cheats.c, cheat_saves.c): the store and
its persistence, the master switch, the hooks' answers and the save marks.
docs/PORT_OPTIONS.md "Cheats"."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
OPTIONS = ROOT / "port" / "options"
CFG = "port_options.cfg"


@pytest.fixture(scope="module")
def cheats_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("cheats") / "cheats_test"
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_PORT_OPTIONS_TEST", "-DDW3_CHEATS", "-o", str(out),
           str(TESTS / "cheats_main.c"), str(OPTIONS / "port_options.c"),
           str(OPTIONS / "options_cfg.c"), str(OPTIONS / "cheats.c"),
           str(OPTIONS / "cheat_saves.c"), str(ROOT / "port" / "audio" / "volume.c"),
           str(ROOT / "port" / "memcard" / "memcard_host.c"),
           str(ROOT / "port" / "memcard" / "save_convert.c"),
           f"-I{ROOT / 'port' / 'include'}", f"-I{ROOT / 'port' / 'memcard'}",
           f"-I{OPTIONS}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture()
def dirs(tmp_path: Path):
    cfg, mem = tmp_path / "cfg", tmp_path / "mem"
    cfg.mkdir()
    mem.mkdir()
    return cfg, mem


def run(binary: Path, dirs, *steps: str, env: dict | None = None):
    cfg, mem = dirs
    e = {k: v for k, v in os.environ.items()
         if not k.startswith("DW3_")}
    e.update(env or {})
    r = subprocess.run([str(binary), str(cfg), str(mem), *steps],
                       capture_output=True, text=True, env=e)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip().splitlines(), r.stderr


def test_defaults_everything_off(cheats_bin, dirs):
    out, err = run(cheats_bin, dirs, "show", "getenv", "DW3_FORCE_ITEMS",
                   "battle", "win", "armed", "marked")
    assert out == ["master=0 fill=0 win=0 battle=0 saves=0 open=0 cursor=0 options=0",
                   "getenv=(null)", "win=0", "armed=0", "marked=0"]
    assert "dw3-cheats: cheats off (instant win off)" in err
    assert not (dirs[0] / CFG).exists()


def test_square_opens_and_triangle_returns_to_options(cheats_bin, dirs):
    out, err = run(cheats_bin, dirs, "options", "show", "square", "show",
                   "back", "show", "sounds")
    assert out[0].endswith("open=0 cursor=0 options=1")
    assert out[1].endswith("open=1 cursor=0 options=1")
    assert out[2].endswith("open=0 cursor=0 options=1")
    assert out[3] == "cursor=0 select=1 cancel=1"
    assert "dw3-cheats: open (master off" in err
    assert "dw3-cheats: close (back to Options)" in err


def test_master_off_greys_the_rows(cheats_bin, dirs):
    # Rows 1..3 do nothing while the master switch is off (no sound either).
    out, err = run(cheats_bin, dirs, "options", "square", "row", "1", "press",
                   "row", "2", "press", "row", "3", "press", "row", "4", "press",
                   "show", "sounds")
    assert out[0].startswith("master=0 fill=0 win=0 battle=0 saves=0 open=1")
    assert out[1] == "cursor=4 select=1 cancel=0"
    assert not (dirs[0] / CFG).exists()


def test_toggles_persist_one_shots_do_not(cheats_bin, dirs):
    run(cheats_bin, dirs, "options", "square", "press", "row", "1", "press",
        "row", "2", "press", "row", "3", "press", "show")
    text = (dirs[0] / CFG).read_text()
    assert "cheat_enabled=1\n" in text and "cheat_instant_win=1\n" in text
    assert text.rstrip().endswith("end=1")
    # The Options rows are still written alongside.
    assert "widescreen=1\n" in text
    out, err = run(cheats_bin, dirs, "show")
    assert out == ["master=1 fill=0 win=1 battle=0 saves=0 open=0 cursor=0 options=0"]
    assert "dw3-cheats: cheats ON (instant win on)" in err


def test_master_off_disarms(cheats_bin, dirs):
    out, _ = run(cheats_bin, dirs, "options", "square", "press", "row", "1",
                 "press", "row", "3", "press", "show", "row", "0", "press", "show",
                 "getenv", "DW3_FORCE_ITEMS", "armed")
    assert out[0].startswith("master=1 fill=1 win=0 battle=1")
    assert out[1].startswith("master=0 fill=0 win=0 battle=0")
    assert out[2:] == ["getenv=(null)", "armed=0"]


def test_bad_values_read_as_off(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=7\ncheat_instant_win=x\nend=1\n")
    out, err = run(cheats_bin, dirs, "show")
    assert out[0].startswith("master=0 fill=0 win=0")
    assert "cheat_enabled=7 is not valid" in err


def test_fill_bag_answers_the_items_hook_once(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\nend=1\n")
    out, err = run(cheats_bin, dirs, "getenv", "DW3_FORCE_ITEMS", "marked",
                   "options", "square", "row", "1", "press", "close",
                   "getenv", "DW3_FORCE_WIN", "getenv", "DW3_FORCE_ITEMS",
                   "getenv", "DW3_FORCE_ITEMS", "marked")
    assert out == ["getenv=(null)", "marked=0", "getenv=(null)",
                   "getenv=43-359:99", "getenv=(null)", "marked=1"]
    assert "fill bag: items 43..359 x99 (used up)" in err


def test_the_variable_wins(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\ncheat_instant_win=1\nend=1\n")
    out, _ = run(cheats_bin, dirs, "getenv", "DW3_FORCE_ITEMS", "battle", "win",
                 "marked",
                 env={"DW3_FORCE_ITEMS": "1-402", "DW3_FORCE_WIN": "1",
                      "DW3_CHEATS_ARM": "fill_bag"})
    # DW3_FORCE_ITEMS answers itself and the one-shot stays ready; with
    # DW3_FORCE_WIN set its own hook ends the battle, not the cheat.
    assert out == ["getenv=1-402", "win=0", "marked=0"]


def test_instant_win_once_per_battle(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\ncheat_instant_win=1\nend=1\n")
    out, err = run(cheats_bin, dirs, "battle", "win", "win", "battle", "win",
                   "marked")
    assert out == ["win=1", "win=0", "win=1", "marked=1"]
    assert "instant win (battle 1 of this run)" in err
    assert "instant win (battle 2 of this run)" in err
    # Master off: nothing, the toggle stays saved.
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=0\ncheat_instant_win=1\nend=1\n")
    out, _ = run(cheats_bin, dirs, "battle", "win", "show")
    assert out[0] == "win=0" and out[1].startswith("master=0 fill=0 win=1")


def test_battle_now_until_a_battle_starts(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\nend=1\n")
    out, err = run(cheats_bin, dirs, "armed", "marked", "started", "armed",
                   env={"DW3_CHEATS_ARM": "battle_now"})
    assert out == ["armed=1", "marked=0", "armed=0"]
    assert "DW3_CHEATS_ARM=battle_now (fill bag off, battle now ready)" in err
    assert "battle now: a random battle starts on this footstep (used up)" in err
    out, _ = run(cheats_bin, dirs, "marked", "started", "marked",
                 env={"DW3_CHEATS_ARM": "battle_now"})
    assert out == ["marked=0", "marked=1"]


def test_soak_rearm_arms_the_one_shots_again(cheats_bin, dirs):
    """P5.1: the PC soak driver (platforms/pc/src/mem_sampler.c) arms the
    one-shots again in the middle of a run, with a DW3_CHEATS_ARM spec."""
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\nend=1\n")
    out, _ = run(cheats_bin, dirs, "armed", "rearm", "battle_now", "armed",
                 "started", "armed", "rearm", "battle_now", "armed",
                 "rearm", "warp=0x206", "warp", "warp",
                 "rearm", "item=47:7", "item", "item")
    assert out == ["armed=0", "armed=1", "armed=0", "armed=1",
                   "warp=1 mode=0x206", "warp=0 mode=0x0",
                   "item=1 id=47 count=7", "item=0 id=0 count=0"]


def test_arm_needs_the_master_switch(cheats_bin, dirs):
    out, _ = run(cheats_bin, dirs, "armed", "getenv", "DW3_FORCE_ITEMS",
                 env={"DW3_CHEATS_ARM": "fill_bag,battle_now"})
    assert out == ["armed=0", "getenv=(null)"]


def test_save_marks(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\ncheat_instant_win=1\nend=1\n")
    mem = dirs[1]
    # A clean save writes no sidecar.
    run(cheats_bin, dirs, "save", "0", "0", "1")
    assert not (mem / "save0.cheats").exists()
    # A won battle marks the game; its save is marked, by slot and hash.
    out, err = run(cheats_bin, dirs, "battle", "win", "save", "0", "2", "5",
                   "count")
    assert out == ["win=1", "count=1"]
    side = (mem / "save0.cheats").read_text()
    assert side.startswith("# dw3-switch: slots of save0.bin written with cheats")
    assert "slot2=" in side and side.rstrip().endswith("end=1")
    assert "save0.bin slot 2 marked: written with cheats" in err
    # A new run sees it at boot and on the screen.
    out, err = run(cheats_bin, dirs, "show")
    assert out[0].startswith("master=1 fill=0 win=1 battle=0 saves=1")
    assert "save0.bin slot 2 was written with cheats" in err
    # Loading it keeps the game marked; the title clears the mark.
    out, err = run(cheats_bin, dirs, "load", "0", "2", "5", "marked", "title",
                   "marked", "load", "0", "2", "6", "marked")
    assert out == ["marked=1", "marked=0", "marked=0"]
    assert "loaded save0.bin slot 2, which was written with cheats" in err
    # A clean save over the slot removes the mark and the empty sidecar.
    out, err = run(cheats_bin, dirs, "save", "0", "2", "7", "count")
    assert out == ["count=0"]
    assert "slot 2: written without cheats, mark removed" in err
    assert not (mem / "save0.cheats").exists()


def test_replaced_slot_is_not_counted(cheats_bin, dirs):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\ncheat_instant_win=1\nend=1\n")
    mem = dirs[1]
    run(cheats_bin, dirs, "battle", "win", "save", "1", "0", "3",
        "battle", "win", "save", "1", "1", "4")
    out, _ = run(cheats_bin, dirs, "count")
    assert out == ["count=2"]
    # Another build (or a copied save1.bin) rewrites slot 0 without telling us.
    data = bytearray((mem / "save1.bin").read_bytes())
    data[0x200 + 0x80 * 2 + 10] ^= 0xFF
    (mem / "save1.bin").write_bytes(bytes(data))
    out, err = run(cheats_bin, dirs, "count")
    assert out == ["count=1"]
    assert "save1.bin slot 0: the mark no longer matches the slot" in err
    # Its stale mark does not mark a load of the new data either.
    out, _ = run(cheats_bin, dirs, "load", "1", "0", "9", "marked")
    assert out == ["marked=0"]


def test_unwritable_memdir_keeps_playing(cheats_bin, dirs, tmp_path):
    (dirs[0] / CFG).write_text("version=1\ncheat_enabled=1\ncheat_instant_win=1\nend=1\n")
    gone = tmp_path / "missing"
    out, _ = run(cheats_bin, (dirs[0], gone), "battle", "win", "count")
    assert out == ["win=1", "count=0"]


def test_cheats_entry_is_locked_in_game(cheats_bin, dirs):
    """P9.7: the Options screen opened from the field menu greys the Cheats
    entry: Square does nothing there (cheats are armed at the title)."""
    out, err = run(cheats_bin, dirs, "ingame", "square", "show")
    assert out == ["master=0 fill=0 win=0 battle=0 saves=0 open=0 cursor=0 options=1"]
    assert "dw3-port-options: cheats are locked in game" in err
    assert "dw3-cheats: open" not in err
    # The same Square from the title opens the Cheats screen.
    out, err = run(cheats_bin, dirs, "options", "square", "show")
    assert out[0].startswith("master=0") and " open=1 " in out[0]
