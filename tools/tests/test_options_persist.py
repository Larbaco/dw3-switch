"""P9.4 option persistence: the file store (port/options/options_cfg.c) and
the registry that loads and saves it (port/options/port_options.c)."""

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


def build(out: Path, *sources: Path, defs=()) -> Path:
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           *defs, "-o", str(out), *map(str, sources),
           f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def cfg_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("optcfg") / "options_cfg_test",
                 TESTS / "options_cfg_main.c", OPTIONS / "options_cfg.c")


@pytest.fixture(scope="module")
def region_bin(tmp_path_factory) -> Path:
    """The registry as the combined image's game copies compile it: the Region
    at start row (P51 item 4) is listed and writes the top-level file."""
    return build(tmp_path_factory.mktemp("optregion") / "options_region_test",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c", OPTIONS / "button_map.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 ROOT / "port" / "region" / "dw3_region.c", TESTS / "region_row_stubs.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST", "-DDW3_REGION_IMAGE=1",
                       f"-I{ROOT / 'port' / 'region'}"])


@pytest.fixture(scope="module")
def persist_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("optpersist") / "options_persist_test",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c", OPTIONS / "button_map.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST"])


def session(binary: Path, cfgdir: Path, *steps: str):
    """One process; returns (stdout lines, stderr)."""
    r = subprocess.run([str(binary), str(cfgdir), *steps],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip().splitlines(), r.stderr


def toggle(binary, cfgdir, row: int, times: int = 1):
    steps = ["open", "row", str(row)] + ["press"] * times + ["close"]
    session(binary, cfgdir, *steps)


def values(binary, cfgdir) -> str:
    out, _ = session(binary, cfgdir, "show")
    return out[-1]


def test_cfg_store(cfg_bin: Path, tmp_path: Path):
    r = subprocess.run([str(cfg_bin), str(tmp_path)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: options_cfg" in r.stdout


def test_defaults_without_a_file(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=0"]
    assert "no " in err and CFG in err
    # Reading alone never creates the file.
    assert not (tmp_path / CFG).exists()


def test_toggles_survive_a_restart(persist_bin: Path, tmp_path: Path):
    toggle(persist_bin, tmp_path, 0)        # widescreen off
    toggle(persist_bin, tmp_path, 1, 2)     # fast-forward 2x -> 3x -> 4x
    toggle(persist_bin, tmp_path, 2)        # restart hotkey on
    text = (tmp_path / CFG).read_text()
    assert text.splitlines()[0].startswith("#")
    assert "version=1\n" in text
    assert "widescreen=0\n" in text
    assert "fast_forward=4\n" in text
    assert "restart_hotkey=1\n" in text
    assert text.endswith("end=1\n")
    assert not (tmp_path / (CFG + ".tmp")).exists()
    # "Restart the host": a new process reads the same values.
    assert values(persist_bin, tmp_path) == \
        "widescreen=0 fast_forward=4 restart_hotkey=1"
    # Cycling Fast-forward back to OFF is saved as 0, not forgotten.
    toggle(persist_bin, tmp_path, 1)
    assert values(persist_bin, tmp_path) == \
        "widescreen=0 fast_forward=0 restart_hotkey=1"


def test_saved_on_each_change_without_closing(persist_bin: Path, tmp_path: Path):
    # A HOME exit or kill while the screen is still open loses nothing.
    session(persist_bin, tmp_path, "open", "row", "2", "press")
    assert "restart_hotkey=1\n" in (tmp_path / CFG).read_text()


def test_no_write_without_a_change(persist_bin: Path, tmp_path: Path):
    session(persist_bin, tmp_path, "open", "row", "1", "close")
    assert not (tmp_path / CFG).exists()


def test_bad_values_fall_back_to_defaults(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(
        "version=1\nwidescreen=maybe\nfast_forward=9\nrestart_hotkey=1\nend=1\n")
    out, err = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=1"]
    assert "widescreen=maybe is not valid" in err
    assert "fast_forward=9 is not valid" in err
    # fast_forward=1 is not a speed (OFF is 0), 2 is.
    (tmp_path / CFG).write_text("fast_forward=1\n")
    assert values(persist_bin, tmp_path).endswith("fast_forward=2 restart_hotkey=0")
    (tmp_path / CFG).write_text("fast_forward=0\nwidescreen=0\n")
    assert values(persist_bin, tmp_path) == \
        "widescreen=0 fast_forward=0 restart_hotkey=0"


def test_garbage_file_means_defaults(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_bytes(bytes(range(256)) * 4)
    out, _ = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=0"]
    (tmp_path / CFG).write_bytes(b"")
    out, _ = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=0"]
    (tmp_path / CFG).write_bytes(b"x" * 10000)   # too large: not ours
    out, err = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=0"]
    assert "cannot read" in err
    # A later change replaces the unreadable file.
    session(persist_bin, tmp_path, "open", "row", "2", "press")
    assert values(persist_bin, tmp_path).endswith("restart_hotkey=1")


def test_unknown_keys_and_newer_version_are_kept(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(
        "version=3\nwidescreen=0\ntexture_pack=my pack\nfuture=1\nend=1\n")
    out, err = session(persist_bin, tmp_path, "show", "open", "row", "2", "press")
    assert out == ["widescreen=0 fast_forward=2 restart_hotkey=0"]
    assert "newer than 1" in err
    text = (tmp_path / CFG).read_text()
    assert "version=3\n" in text
    assert "texture_pack=my pack\n" in text
    assert "future=1\n" in text
    assert "restart_hotkey=1\n" in text
    assert text.index("widescreen") < text.index("texture_pack")  # order kept


def test_missing_version_and_comments_still_load(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("# edited by hand\n\n  fast_forward = 3 \n")
    assert values(persist_bin, tmp_path) == \
        "widescreen=1 fast_forward=3 restart_hotkey=0"


def test_recovery_from_an_interrupted_save(persist_bin: Path, tmp_path: Path):
    # Died between "remove old" and "rename": only a complete .tmp is left.
    (tmp_path / (CFG + ".tmp")).write_text(
        "version=1\nwidescreen=0\nfast_forward=3\nrestart_hotkey=1\nend=1\n")
    out, err = session(persist_bin, tmp_path, "show")
    assert out == ["widescreen=0 fast_forward=3 restart_hotkey=1"]
    assert "recovered" in err
    # D2: the recovered file has no defaults_gen, so it is migrated and written
    # back at once: the real file exists again.
    assert (tmp_path / CFG).exists()
    (tmp_path / CFG).unlink()
    # A torn temp file is not trusted.
    (tmp_path / (CFG + ".tmp")).write_text("version=1\nwidescreen=0\nfast_")
    assert values(persist_bin, tmp_path) == \
        "widescreen=1 fast_forward=2 restart_hotkey=0"
    # The next save cleans up and wins over a stale temp file.
    session(persist_bin, tmp_path, "open", "row", "0", "press")
    assert not (tmp_path / (CFG + ".tmp")).exists()
    assert (tmp_path / CFG).read_text().count("widescreen=0\n") == 1


def test_environment_override_is_not_saved(persist_bin: Path, tmp_path: Path):
    # The file says widescreen=1 (default). DW3_WIDESCREEN=0 forces it off for
    # this run only: an unrelated toggle must not write that choice.
    session(persist_bin, tmp_path, "env", "0", "0", "open", "row", "2", "press")
    text = (tmp_path / CFG).read_text()
    assert "widescreen=1\n" in text and "restart_hotkey=1\n" in text
    # Toggling the forced row itself is the user's choice: saved.
    session(persist_bin, tmp_path, "env", "0", "0", "open", "row", "0", "press")
    assert "widescreen=1\n" in (tmp_path / CFG).read_text()  # was forced 0 -> 1
    session(persist_bin, tmp_path, "env", "0", "1", "open", "row", "0", "press")
    assert "widescreen=0\n" in (tmp_path / CFG).read_text()  # forced 1 -> 0
    # DW3_TURBO=4 (fast-forward) likewise.
    (tmp_path / CFG).unlink()
    session(persist_bin, tmp_path, "env", "1", "4", "open", "row", "0", "press")
    assert "fast_forward=2\n" in (tmp_path / CFG).read_text()


def test_no_config_dir_is_session_only(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path / "nope", "show", "open", "row", "2",
                       "press", "show")
    assert out[-1] == "widescreen=1 fast_forward=2 restart_hotkey=1"
    assert not (tmp_path / "nope").exists()
    # No persist_init at all (the host tests): nothing is written anywhere.
    out, _ = session(persist_bin, tmp_path, "noinit", "open", "row", "2", "press")
    assert not (tmp_path / CFG).exists()


@pytest.mark.skipif(os.geteuid() == 0, reason="root ignores directory modes")
def test_unwritable_dir_keeps_the_old_file(persist_bin: Path, tmp_path: Path):
    toggle(persist_bin, tmp_path, 2)
    before = (tmp_path / CFG).read_text()
    tmp_path.chmod(stat.S_IRUSR | stat.S_IXUSR)
    try:
        out, err = session(persist_bin, tmp_path, "open", "row", "0", "press",
                           "show", "close")
        assert out == ["widescreen=0 fast_forward=2 restart_hotkey=1"]  # live value kept
        assert "save to" in err and "failed" in err
    finally:
        tmp_path.chmod(stat.S_IRWXU)
    assert (tmp_path / CFG).read_text() == before
    assert not (tmp_path / (CFG + ".tmp")).exists()


# --- P6.17 Music / SFX volume rows -----------------------------------------

def volumes(binary, cfgdir) -> str:
    out, _ = session(binary, cfgdir, "volumes")
    return out[-1]


def test_volume_defaults_are_the_original_mix(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "volumes", "gains")
    assert out[0] == "music_volume=10 sfx_volume=10"
    # persist_init hands the defaults to the mixer: step 10 = unity (0x8000)
    assert out[1] == "music_step=10 sfx_step=10 music_gain=32768 sfx_gain=32768 calls=1"
    assert not (tmp_path / CFG).exists()


def test_volume_steps_left_right_and_confirm(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "open", "row", "3",
                     "left", "left", "left", "volumes", "gains")
    assert out[0] == "music_volume=7 sfx_volume=10"
    assert out[1].startswith("music_step=7 sfx_step=10 music_gain=16056 ")
    # Right goes back up and stops at 10.
    out, _ = session(persist_bin, tmp_path, "open", "row", "3",
                     *["right"] * 6, "volumes")
    assert out[0] == "music_volume=10 sfx_volume=10"
    # Left stops at 0 (silent), and does not go negative.
    out, _ = session(persist_bin, tmp_path, "open", "row", "4",
                     *["left"] * 12, "volumes", "gains")
    assert out[0] == "music_volume=10 sfx_volume=0"
    assert out[1].startswith("music_step=10 sfx_step=0 music_gain=32768 sfx_gain=0 ")
    # Confirm steps up and wraps 10 -> 0, like the Fast-forward row wraps.
    out, _ = session(persist_bin, tmp_path, "open", "row", "4", "press",
                     "volumes")
    assert out[0] == "music_volume=10 sfx_volume=1"
    out, _ = session(persist_bin, tmp_path, "open", "row", "4",
                     *["press"] * 9, "volumes")
    assert out[0] == "music_volume=10 sfx_volume=10"
    out, _ = session(persist_bin, tmp_path, "open", "row", "4", "press", "volumes")
    assert out[0] == "music_volume=10 sfx_volume=0"


def test_left_right_do_nothing_on_the_switch_rows(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "open", "left", "right", "row", "1",
                     "left", "right", "row", "2", "left", "right", "show", "volumes")
    assert out == ["widescreen=1 fast_forward=2 restart_hotkey=0",
                   "music_volume=10 sfx_volume=10"]
    assert not (tmp_path / CFG).exists()


def test_volumes_round_trip_and_are_saved_on_each_change(persist_bin: Path, tmp_path: Path):
    session(persist_bin, tmp_path, "open", "row", "3", "left", "left", "left")
    text = (tmp_path / CFG).read_text()   # saved without leaving the screen
    assert "music_volume=7\n" in text and "sfx_volume=10\n" in text
    assert text.endswith("end=1\n")
    session(persist_bin, tmp_path, "open", "row", "4", *["left"] * 7, "close")
    assert volumes(persist_bin, tmp_path) == "music_volume=7 sfx_volume=3"
    # A new process starts the mixer at the saved steps.
    out, _ = session(persist_bin, tmp_path, "gains")
    assert out[0] == "music_step=7 sfx_step=3 music_gain=16056 sfx_gain=2949 calls=1"
    # The three switches are untouched by volume changes and vice versa.
    toggle(persist_bin, tmp_path, 0)
    assert values(persist_bin, tmp_path) == "widescreen=0 fast_forward=2 restart_hotkey=0"
    assert volumes(persist_bin, tmp_path) == "music_volume=7 sfx_volume=3"


def test_bad_volume_values_fall_back_to_the_default(persist_bin: Path, tmp_path: Path):
    for bad in ("11", "-1", "loud", "", "5.5", "0x5"):
        (tmp_path / CFG).write_text(f"version=1\nmusic_volume={bad}\nsfx_volume=4\nend=1\n")
        out, err = session(persist_bin, tmp_path, "volumes")
        assert out == ["music_volume=10 sfx_volume=4"], bad
        assert "music_volume=" in err and "is not valid" in err, bad
    (tmp_path / CFG).write_text("music_volume=0\nsfx_volume=0\n")   # 0 = silent is valid
    assert volumes(persist_bin, tmp_path) == "music_volume=0 sfx_volume=0"
    (tmp_path / CFG).write_text("music_volume = 6 \n")
    assert volumes(persist_bin, tmp_path) == "music_volume=6 sfx_volume=10"


def test_older_file_without_volume_keys_keeps_the_default(persist_bin: Path, tmp_path: Path):
    # A P9.4 file written before P6.17: the new keys are simply missing.
    (tmp_path / CFG).write_text(
        "version=1\nwidescreen=0\nfast_forward=3\nrestart_hotkey=1\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "show", "volumes")
    assert out == ["widescreen=0 fast_forward=3 restart_hotkey=1",
                   "music_volume=10 sfx_volume=10"]
    # The first change adds the keys and keeps the rest.
    session(persist_bin, tmp_path, "open", "row", "3", "left")
    text = (tmp_path / CFG).read_text()
    assert "widescreen=0\n" in text and "restart_hotkey=1\n" in text
    assert "music_volume=9\n" in text and "sfx_volume=10\n" in text


def test_environment_volume_override_is_not_saved(persist_bin: Path, tmp_path: Path):
    env = {**os.environ, "DW3_MUSIC_VOLUME": "2", "DW3_SFX_VOLUME": "bogus"}
    r = subprocess.run([str(persist_bin), str(tmp_path), "volumes", "gains",
                        "open", "row", "2", "press"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    out = r.stdout.strip().splitlines()
    assert out[0] == "music_volume=2 sfx_volume=10"      # forced for this run
    assert out[1].startswith("music_step=2 sfx_step=10 ")
    # An unrelated change must not write the forced value as the user's choice.
    text = (tmp_path / CFG).read_text()
    assert "music_volume=10\n" in text and "restart_hotkey=1\n" in text
    # Changing the forced row itself is the user's choice and is saved.
    r = subprocess.run([str(persist_bin), str(tmp_path), "open", "row", "3", "left"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert "music_volume=1\n" in (tmp_path / CFG).read_text()
    # Forced even without a usable config dir.
    r = subprocess.run([str(persist_bin), str(tmp_path / "nope"), "volumes", "gains"],
                       capture_output=True, text=True, env=env)
    assert r.stdout.strip().splitlines()[0] == "music_volume=2 sfx_volume=10"


def test_volume_keys_are_in_the_written_file_order(persist_bin: Path, tmp_path: Path):
    toggle(persist_bin, tmp_path, 3)
    keys = [l.split("=")[0] for l in (tmp_path / CFG).read_text().splitlines()
            if "=" in l and not l.startswith("#")]
    assert keys == ["version", "widescreen", "fast_forward", "restart_hotkey",
                    "music_volume", "sfx_volume", "encounter_rate", "text_speed",
                    "battle_menu_memory", "autosave", "button_layout", "turbo_music", "switch_glyphs", "map_screen",
                    "items_equip_from_items", "items_equip_stays", "items_bag_view",
                    "items_use_stays", "items_partner_switch", "show_version", "map_mod",
                    "save_store", "stage_sweep", "stage_sweep_frames", "map_scaler",
                    "defaults_gen", "end"]


# --- P6.18 Encounter rate / Text speed rows ---------------------------------

def pacing(binary, cfgdir) -> str:
    out, _ = session(binary, cfgdir, "pacing")
    return out[-1]


def test_pacing_defaults_are_the_original_game(persist_bin: Path, tmp_path: Path):
    assert pacing(persist_bin, tmp_path) == "encounter_rate=0 text_speed=0"
    assert not (tmp_path / CFG).exists()


def test_pacing_steps_left_right_and_confirm(persist_bin: Path, tmp_path: Path):
    def fresh(*steps):
        (tmp_path / CFG).unlink(missing_ok=True)      # every session starts at the defaults
        return session(persist_bin, tmp_path, *steps)

    # Encounter rate is stored 0 = 1x, 1 = 0.5x, 2 = OFF, 3 = 1.5x, 4 = 2x and
    # stepped in the order OFF, 0.5x, 1x, 1.5x, 2x: from the default 1x, Right
    # goes up to 1.5x and 2x and stops there; Left goes down to 0.5x and OFF.
    out, _ = fresh("open", "row", "5", "right", "pacing")
    assert out[0] == "encounter_rate=3 text_speed=0"      # 1.5x
    out, _ = fresh("open", "row", "5", "right", "right", "pacing")
    assert out[0] == "encounter_rate=4 text_speed=0"      # 2x
    out, _ = fresh("open", "row", "5", *["right"] * 6, "pacing")
    assert out[0] == "encounter_rate=4 text_speed=0"      # stops at 2x
    out, _ = fresh("open", "row", "5", "left", "pacing")
    assert out[0] == "encounter_rate=1 text_speed=0"      # 0.5x
    out, _ = fresh("open", "row", "5", "left", "left", "pacing")
    assert out[0] == "encounter_rate=2 text_speed=0"      # OFF
    out, _ = fresh("open", "row", "5", *["left"] * 6, "pacing")
    assert out[0] == "encounter_rate=2 text_speed=0"      # stops at OFF
    # Confirm steps up and wraps after the last mode: 1x, 1.5x, 2x, OFF, 0.5x, 1x.
    seq = [3, 4, 2, 1, 0, 3]
    for n, want in enumerate(seq, start=1):
        out, _ = fresh("open", "row", "5", *["press"] * n, "pacing")
        assert out[0] == f"encounter_rate={want} text_speed=0", n
    # Text speed keeps its own order (Normal, Fast, Instant) and wraps.
    out, _ = fresh("open", "row", "6", "press", "pacing")
    assert out[0] == "encounter_rate=0 text_speed=1"
    out, _ = fresh("open", "row", "6", "press", "press", "pacing")
    assert out[0] == "encounter_rate=0 text_speed=2"
    out, _ = fresh("open", "row", "6", "press", "press",
                     "press", "pacing")
    assert out[0] == "encounter_rate=0 text_speed=0"      # 0 -> 1 -> 2 -> 0
    out, _ = fresh("open", "row", "6", *["right"] * 4, "pacing")
    assert out[0] == "encounter_rate=0 text_speed=2"      # Text speed stops at Instant


def test_pacing_round_trip_saved_on_each_change(persist_bin: Path, tmp_path: Path):
    session(persist_bin, tmp_path, "open", "row", "5", "left", "left",
            "row", "6", "right")
    text = (tmp_path / CFG).read_text()   # saved without leaving the screen
    assert "encounter_rate=2\n" in text and "text_speed=1\n" in text
    assert text.endswith("end=1\n")
    assert pacing(persist_bin, tmp_path) == "encounter_rate=2 text_speed=1"
    # The other rows are untouched and keep their defaults.
    assert values(persist_bin, tmp_path) == "widescreen=1 fast_forward=2 restart_hotkey=0"
    assert volumes(persist_bin, tmp_path) == "music_volume=10 sfx_volume=10"


def test_encounter_1_5x_and_2x_persist_and_old_values_keep_their_meaning(
        persist_bin: Path, tmp_path: Path):
    # 3 = 1.5x and 4 = 2x are saved as such, written back unchanged on an
    # unrelated change, and read by the next process (the stored values of
    # P6.18, 0 / 1 / 2, are not renumbered).
    for stored in (0, 1, 2, 3, 4):
        (tmp_path / CFG).write_text(
            f"version=1\nencounter_rate={stored}\ntext_speed=1\nend=1\n")
        out, err = session(persist_bin, tmp_path, "pacing")
        assert out == [f"encounter_rate={stored} text_speed=1"], stored
        assert "is not valid" not in err, stored
        toggle(persist_bin, tmp_path, 2)       # an unrelated row
        assert f"encounter_rate={stored}\n" in (tmp_path / CFG).read_text(), stored
        assert pacing(persist_bin, tmp_path) == f"encounter_rate={stored} text_speed=1"
    # Reaching them on the screen saves them at once.
    (tmp_path / CFG).unlink()
    session(persist_bin, tmp_path, "open", "row", "5", "right", "right")
    assert "encounter_rate=4\n" in (tmp_path / CFG).read_text()
    assert pacing(persist_bin, tmp_path) == "encounter_rate=4 text_speed=0"
    session(persist_bin, tmp_path, "open", "row", "5", "left")
    assert "encounter_rate=3\n" in (tmp_path / CFG).read_text()
    # A file from P6.18 (0.5x, Off) steps from where it is, in the new order.
    (tmp_path / CFG).write_text("version=1\nencounter_rate=1\ntext_speed=0\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "open", "row", "5", "right", "pacing")
    assert out[0] == "encounter_rate=0 text_speed=0"      # 0.5x -> 1x
    (tmp_path / CFG).write_text("version=1\nencounter_rate=2\ntext_speed=0\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "open", "row", "5", "right", "pacing")
    assert out[0] == "encounter_rate=1 text_speed=0"      # OFF -> 0.5x
    (tmp_path / CFG).write_text("version=1\nencounter_rate=2\ntext_speed=0\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "open", "row", "5", "press", "press",
                     "press", "press", "press", "pacing")
    assert out[0] == "encounter_rate=2 text_speed=0"      # a full lap ends on OFF again


def test_bad_pacing_values_fall_back_to_the_default(persist_bin: Path, tmp_path: Path):
    for bad in ("5", "-1", "fast", "", "1.5", "0x1", "9"):
        (tmp_path / CFG).write_text(f"version=1\nencounter_rate={bad}\ntext_speed=2\nend=1\n")
        out, err = session(persist_bin, tmp_path, "pacing")
        assert out == ["encounter_rate=0 text_speed=2"], bad
        assert "encounter_rate=" in err and "is not valid" in err, bad
    for bad in ("3", "4", "-1", "fast", "", "1.5", "0x1"):
        (tmp_path / CFG).write_text(f"version=1\nencounter_rate=2\ntext_speed={bad}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "pacing")
        assert out == ["encounter_rate=2 text_speed=0"], bad
        assert "text_speed=" in err and "is not valid" in err, bad


def test_older_file_without_pacing_keys_keeps_the_default(persist_bin: Path, tmp_path: Path):
    # A file written by P6.17 (no pacing keys): everything else is read as before.
    (tmp_path / CFG).write_text(
        "version=1\nwidescreen=0\nfast_forward=3\nrestart_hotkey=1\n"
        "music_volume=7\nsfx_volume=3\nend=1\n")
    out, err = session(persist_bin, tmp_path, "show", "volumes", "pacing")
    assert out == ["widescreen=0 fast_forward=3 restart_hotkey=1",
                   "music_volume=7 sfx_volume=3",
                   "encounter_rate=0 text_speed=0"]
    assert "is not valid" not in err
    # And one from P9.4 (no volume keys either).
    (tmp_path / CFG).write_text("version=1\nwidescreen=0\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "volumes", "pacing")
    assert out == ["music_volume=10 sfx_volume=10", "encounter_rate=0 text_speed=0"]
    # The first change adds the new keys and keeps the rest.
    session(persist_bin, tmp_path, "open", "row", "6", "right")
    text = (tmp_path / CFG).read_text()
    assert "widescreen=0\n" in text
    assert "encounter_rate=0\n" in text and "text_speed=1\n" in text


def test_environment_pacing_override_is_not_saved(persist_bin: Path, tmp_path: Path):
    env = {**os.environ, "DW3_ENCOUNTER_RATE": "2", "DW3_TEXT_SPEED": "bogus"}
    r = subprocess.run([str(persist_bin), str(tmp_path), "pacing",
                        "open", "row", "2", "press"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert r.stdout.strip().splitlines()[0] == "encounter_rate=2 text_speed=0"  # forced; bad one ignored
    # An unrelated change must not write the forced value as the user's choice.
    text = (tmp_path / CFG).read_text()
    assert "encounter_rate=0\n" in text and "restart_hotkey=1\n" in text
    # Changing the forced row itself is the user's choice and is saved.
    r = subprocess.run([str(persist_bin), str(tmp_path), "open", "row", "5", "right"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert "encounter_rate=1\n" in (tmp_path / CFG).read_text()      # OFF -> 0.5x
    # Forced even without a usable config dir.
    r = subprocess.run([str(persist_bin), str(tmp_path / "nope"), "pacing"],
                       capture_output=True, text=True, env=env)
    assert r.stdout.strip().splitlines()[0] == "encounter_rate=2 text_speed=0"
    # 3 (1.5x) and 4 (2x) can be forced too; 5 is ignored.
    for forced, want in (("3", 3), ("4", 4), ("5", 0)):
        r = subprocess.run([str(persist_bin), str(tmp_path / "nope"), "pacing"],
                           capture_output=True, text=True,
                           env={**os.environ, "DW3_ENCOUNTER_RATE": forced})
        assert r.stdout.strip().splitlines()[0] == f"encounter_rate={want} text_speed=0", forced


# --- Options screen menu sound feedback ----------------------------------------------

def sounds(err: str) -> list[str]:
    """The `sound <kind> (sfx volume N)` log lines, in order, as 'kind N'."""
    import re
    return [f"{m[0]} {m[1]}" for m in
            re.findall(r"dw3-port-options: sound (\w+) \(sfx volume (\d+)\)", err)]


def test_sound_on_value_change_confirm_and_left_right(persist_bin: Path, tmp_path: Path):
    # Confirm on a switch row, a stepped row and the Fast-forward row: the
    # game's choice sound (select); the cursor moves between rows tick.
    # (P9.9: the first Confirm opens a group, and plays it too.)
    _, err = session(persist_bin, tmp_path, "open", "press")
    assert sounds(err) == ["select 10"]
    _, err = session(persist_bin, tmp_path, "open", "press", "press")
    assert sounds(err) == ["select 10", "select 10"]
    _, err = session(persist_bin, tmp_path, "open", "down", "press")
    assert sounds(err) == ["cursor 10", "select 10"]
    # Encounter rate: two groups down, open Gameplay, one row down, Confirm (D1: Restart
    # hotkey left Gameplay, so Encounter rate is its second row).
    _, err = session(persist_bin, tmp_path, "open", "nav", "5", "press")
    assert sounds(err)[-1] == "select 10" and sounds(err).count("cursor 10") == 3
    # Left / Right on a stepped row change the value, so they play it too.
    _, err = session(persist_bin, tmp_path, "open", "row", "6", "right", "left")
    assert sounds(err)[-2:] == ["select 10", "select 10"]


def test_no_sound_when_nothing_changes(persist_bin: Path, tmp_path: Path):
    # Up on the first row, Down on the last, and Left / Right on a row that
    # does not take them or that is already at its end: silent, as the game's
    # own menus are at the end of a list.
    _, err = session(persist_bin, tmp_path, "open", "up", "left", "right")
    assert sounds(err) == []
    # (The cursor and group-open sounds of the walk to the row are expected.)
    cur, sel = "cursor 10", "select 10"
    _, err = session(persist_bin, tmp_path, "open", "nav", "3", "right")
    assert sounds(err) == [cur, sel]                 # Music volume is already at 10
    _, err = session(persist_bin, tmp_path, "open", "nav", "6", "left")
    assert sounds(err) == [cur] * 2 + [sel] + [cur] * 2   # Text speed is already Normal
    _, err = session(persist_bin, tmp_path, "open", "nav", "7", "left")
    assert sounds(err) == [cur] * 2 + [sel] + [cur] * 3   # Battle cursor is already Off
    _, err = session(persist_bin, tmp_path, "open", "nav", "23", "down")
    assert sounds(err) == [cur] * 5 + [sel] + [cur]  # Shared saves, the last row of Saves
    # The plain open and close calls make no sound; the screen plays Select
    # for the opening from the title menu (decomp patch 0044), not here.
    _, err = session(persist_bin, tmp_path, "open", "close")
    assert sounds(err) == []


def test_cancel_sound_on_back(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "open", "back")
    assert sounds(err) == ["cancel 10"]
    assert "dw3-port-options: close" in err


def test_back_from_a_group_returns_to_the_list_with_the_cursor_kept(
        persist_bin: Path, tmp_path: Path):
    # Back inside a group goes to the list (cancel sound, group close line); a
    # second Back closes the screen. P9.9.
    _, err = session(persist_bin, tmp_path, "open", "down", "press", "back")
    assert sounds(err) == ["cursor 10", "select 10", "cancel 10"]
    assert "dw3-port-options: group open sound (3 row(s), title)" in err
    assert "dw3-port-options: group close sound (back to the group list)" in err
    assert "dw3-port-options: close" not in err.replace("group close", "")
    out, err = session(persist_bin, tmp_path, "open", "down", "press", "back", "back")
    assert sounds(err)[-1] == "cancel 10" and "dw3-port-options: close (resume" in err
    # The list cursor is still on Sound: Confirm opens Sound again.
    _, err = session(persist_bin, tmp_path, "open", "down", "press", "back", "press")
    assert err.count("group open sound") == 2


def assert_apply_then_sound(err: str, applied: str, sound: str) -> None:
    """The last mixer apply is the given one and the last select sound follows it."""
    apply_at = err.rindex("test: audio apply " + applied)
    sound_at = err.rindex("dw3-port-options: sound select (" + sound + ")")
    assert apply_at < sound_at
    assert "test: audio apply" not in err[apply_at + 10:]      # nothing newer


def test_sound_comes_after_the_new_volume_is_applied(persist_bin: Path, tmp_path: Path):
    # SFX volume 10 -> 9: the mixer gets sfx 9 first, then the sound plays, and
    # the line says it plays at the new step (the owner hears the new level).
    _, err = session(persist_bin, tmp_path, "open", "row", "4", "left")
    assert_apply_then_sound(err, "music=10 sfx=9", "sfx volume 9")
    # Confirm wraps 10 -> 0: the sound is then at level 0 (silent by design).
    (tmp_path / CFG).write_text("version=1\nsfx_volume=10\nend=1\n")
    _, err = session(persist_bin, tmp_path, "open", "row", "4", "press")
    assert sounds(err)[-1] == "select 0"
    # A Music volume change plays the sound at the SFX level.
    (tmp_path / CFG).write_text("version=1\nsfx_volume=4\nend=1\n")
    _, err = session(persist_bin, tmp_path, "open", "row", "3", "left")
    assert_apply_then_sound(err, "music=9 sfx=4", "sfx volume 4")



def test_file_only_int_key(persist_bin: Path, tmp_path: Path):
    """A key with no Options row is read from the file, falls back to the
    default when absent or not a number, and survives a rewrite of the file.
    (battle_menu_memory was the example until it became the Battle cursor row;
    it is still readable this way, and `odd` / `missing` are the file-only
    keys now.)"""
    out, _ = session(persist_bin, tmp_path, "fileint", "battle_menu_memory", "0")
    assert out == ["battle_menu_memory=0"]  # no file
    (tmp_path / CFG).write_text(
        "version=1\nbattle_menu_memory=2\nodd=abc\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "fileint", "battle_menu_memory", "0",
                     "fileint", "odd", "5", "fileint", "missing", "7")
    assert out == ["battle_menu_memory=2", "odd=5", "missing=7"]
    toggle(persist_bin, tmp_path, 2)  # the Options screen rewrites the file
    text = (tmp_path / CFG).read_text()
    assert "battle_menu_memory=2\n" in text
    out, _ = session(persist_bin, tmp_path, "fileint", "battle_menu_memory", "0")
    assert out == ["battle_menu_memory=2"]
    out, _ = session(persist_bin, tmp_path, "noinit", "fileint", "battle_menu_memory", "0")
    assert out == ["battle_menu_memory=0"]  # no config dir: the default


# --- P6.32 phase 4: the Full-screen map row (key map_screen, row 15) and the map view's
# file-only binding keys ------------------------------------------------------

def test_map_screen_row_default_toggle_and_relaunch(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "level", "15")
    assert out == ["level15=0"]                       # off with no file
    toggle(persist_bin, tmp_path, 15)                 # Display, second row: on
    assert "map_screen=1\n" in (tmp_path / CFG).read_text()
    out, _ = session(persist_bin, tmp_path, "level", "15", "fileint", "map_screen", "0")
    assert out == ["level15=1", "map_screen=1"]       # a new process reads it back
    toggle(persist_bin, tmp_path, 15)                 # and off again
    out, _ = session(persist_bin, tmp_path, "level", "15")
    assert out == ["level15=0"]
    assert "map_screen=0\n" in (tmp_path / CFG).read_text()


def test_map_screen_key_of_the_preview_builds_is_read_and_a_bad_value_falls_back(
        persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nmap_screen=1\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "level", "15")
    assert out == ["level15=1"]                       # the file-only key of the previews
    (tmp_path / CFG).write_text("version=1\nmap_screen=2\nend=1\n")
    out, err = session(persist_bin, tmp_path, "level", "15")
    assert out == ["level15=0"]
    assert "map_screen=2 is not valid; using default 0" in err


def test_map_view_binding_keys_survive_the_options_screen(persist_bin: Path, tmp_path: Path):
    """The six binding keys have no row: they are read as text and kept when the
    Options screen (the map_screen row included) rewrites the file."""
    keys = ("map_view_open=r2\nmap_view_zoom_out=square\nmap_view_zoom_in=cross\n"
            "map_view_centre=l3\nmap_view_scaler_prev=l1\nmap_view_scaler_next=r1\n")
    (tmp_path / CFG).write_text("version=1\n" + keys + "end=1\n")
    out, _ = session(persist_bin, tmp_path, "fileval", "map_view_open", "fileval", "map_view_centre",
                     "fileval", "map_view_nothing")
    assert out == ["map_view_open=r2", "map_view_centre=l3", "map_view_nothing=(none)"]
    toggle(persist_bin, tmp_path, 15)                 # the screen rewrites the file
    text = (tmp_path / CFG).read_text()
    for line in keys.splitlines():
        assert line + "\n" in text, line
    assert "map_screen=1\n" in text
    out, _ = session(persist_bin, tmp_path, "fileval", "map_view_zoom_in", "level", "15")
    assert out == ["map_view_zoom_in=cross", "level15=1"]   # a relaunch: both are back
    out, _ = session(persist_bin, tmp_path, "noinit", "fileval", "map_view_open")
    assert out == ["map_view_open=(none)"]            # no config dir: no file keys


# --- P7.9 Battle cursor row (was the file-only key battle_menu_memory) -------------

def battle(binary, cfgdir) -> str:
    out, _ = session(binary, cfgdir, "battle")
    return out[-1]


def test_battle_cursor_defaults_to_off(persist_bin: Path, tmp_path: Path):
    assert battle(persist_bin, tmp_path) == "battle_menu_memory=0"
    assert not (tmp_path / CFG).exists()


def test_battle_cursor_steps_and_confirm_wrap(persist_bin: Path, tmp_path: Path):
    def fresh(*steps):
        for f in tmp_path.glob("port_options.cfg*"):
            f.unlink()
        return session(persist_bin, tmp_path, *steps)

    out, _ = fresh("open", "row", "7", "right", "battle")
    assert out[0] == "battle_menu_memory=1"
    out, _ = fresh("open", "row", "7", "right", "right", "battle")
    assert out[0] == "battle_menu_memory=2"
    out, _ = fresh("open", "row", "7", *["right"] * 5, "battle")
    assert out[0] == "battle_menu_memory=2"          # stops at Command+Tech
    out, _ = fresh("open", "row", "7", "left", "battle")
    assert out[0] == "battle_menu_memory=0"          # stops at Off
    for n, want in ((1, 1), (2, 2), (3, 0), (4, 1)):
        out, _ = fresh("open", "row", "7", *["press"] * n, "battle")
        assert out[0] == f"battle_menu_memory={want}", n   # Confirm wraps 2 -> 0
    # Auto-save (P6.31) is the last row since: Down from Battle cursor goes to
    # it, and Confirm there leaves Battle cursor alone.
    out, _ = fresh("open", "row", "7", "nav", "8", "press", "battle")
    assert out[0] == "battle_menu_memory=0"


def test_battle_cursor_is_the_old_file_key(persist_bin: Path, tmp_path: Path):
    """The key and its values are those of the file-only option: a file written
    by hand for P7.9 is read by the row, and the row writes it back."""
    for stored in (0, 1, 2):
        (tmp_path / CFG).write_text(f"version=1\nbattle_menu_memory={stored}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "battle")
        assert out == [f"battle_menu_memory={stored}"]
        assert "is not valid" not in err
        assert f"battle_menu_memory={stored})" in err        # the load line
    session(persist_bin, tmp_path, "open", "row", "7", "left", "left", "close")
    assert "battle_menu_memory=0\n" in (tmp_path / CFG).read_text()
    # Saved on each change, in the file as the key P7.9 documented.
    session(persist_bin, tmp_path, "open", "row", "7", "right", "right")
    text = (tmp_path / CFG).read_text()
    assert "battle_menu_memory=2\n" in text and text.endswith("end=1\n")
    assert battle(persist_bin, tmp_path) == "battle_menu_memory=2"
    _, err = session(persist_bin, tmp_path, "open", "row", "7", "left")
    assert "dw3-port-options: battle cursor command" in err
    assert "saved " in err and "battle_menu_memory=1)" in err


def test_bad_battle_cursor_value_is_off(persist_bin: Path, tmp_path: Path):
    for bad in ("3", "7", "-1", "on", "", "1.5"):
        (tmp_path / CFG).write_text(f"version=1\nbattle_menu_memory={bad}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "battle")
        assert out == ["battle_menu_memory=0"], bad
        assert "battle_menu_memory=" in err and "is not valid" in err, bad


def test_older_file_without_the_battle_key_keeps_off(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(
        "version=1\nwidescreen=0\nencounter_rate=3\ntext_speed=2\nend=1\n")
    out, err = session(persist_bin, tmp_path, "battle", "pacing")
    assert out == ["battle_menu_memory=0", "encounter_rate=3 text_speed=2"]
    assert "is not valid" not in err
    session(persist_bin, tmp_path, "open", "row", "7", "right")
    text = (tmp_path / CFG).read_text()
    assert "widescreen=0\n" in text and "encounter_rate=3\n" in text
    assert "battle_menu_memory=1\n" in text


def test_battle_cursor_plays_the_choice_sound(persist_bin: Path, tmp_path: Path):
    _, err = session(persist_bin, tmp_path, "open", "row", "7", "right")
    assert sounds(err)[-1].startswith("select ")
    # At the end of the row nothing changes and nothing plays.
    (tmp_path / CFG).write_text("version=1\nbattle_menu_memory=2\nend=1\n")
    _, err = session(persist_bin, tmp_path, "open", "nav", "7", "right")
    assert sounds(err)[-1].startswith("cursor ")      # only the move to the row


# --- P6.31 Auto-save row ------------------------------------------------------

def test_autosave_defaults_to_off_and_writes_no_file(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "autosave")
    assert out == ["autosave=0"]
    assert not (tmp_path / CFG).exists()
    assert "autosave=" not in err                     # nothing is logged while off


def test_autosave_confirm_toggles_and_is_saved(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "open", "row", "8", "press", "autosave")
    assert out == ["autosave=1"]
    assert "dw3-port-options: auto-save enabled" in err
    assert "autosave=1\n" in (tmp_path / CFG).read_text()
    # A new process reads it back, and says so.
    out, err = session(persist_bin, tmp_path, "autosave")
    assert out == ["autosave=1"]
    assert "dw3-options: autosave=1" in err
    # Confirm again turns it off; Left / Right do nothing on a switch row.
    out, err = session(persist_bin, tmp_path, "open", "row", "8", "right", "autosave",
                       "press", "autosave")
    assert out == ["autosave=1", "autosave=0"]
    assert "dw3-port-options: auto-save disabled" in err
    assert "autosave=0\n" in (tmp_path / CFG).read_text()


def test_autosave_bad_values_are_off(persist_bin: Path, tmp_path: Path):
    for bad in ("2", "-1", "on", "", "1.5"):
        (tmp_path / CFG).write_text(f"version=1\nautosave={bad}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "autosave")
        assert out == ["autosave=0"], bad
        assert "autosave=" in err and "is not valid" in err, bad


def test_older_file_without_the_autosave_key_keeps_off(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nwidescreen=0\nbattle_menu_memory=2\nend=1\n")
    out, err = session(persist_bin, tmp_path, "autosave", "battle")
    assert out == ["autosave=0", "battle_menu_memory=2"]
    assert "is not valid" not in err
    session(persist_bin, tmp_path, "open", "row", "8", "press")
    text = (tmp_path / CFG).read_text()
    assert "widescreen=0\n" in text and "battle_menu_memory=2\n" in text
    assert "autosave=1\n" in text


def test_autosave_plays_the_choice_sound(persist_bin: Path, tmp_path: Path):
    _, err = session(persist_bin, tmp_path, "open", "row", "8", "press")
    assert sounds(err)[-1].startswith("select ")


# --- P9.8 Button layout (docs/CONTROLS_BUNDLE.md) ---------------------------


def seed_ps(tmp_path: Path, extra: str = "") -> str:
    """A current-generation file holding the PlayStation layout (D2: the default is
    Nintendo, so the tests of the PlayStation flows say so in the file)."""
    text = f"version=1\ndefaults_gen=2\nbutton_layout=playstation\n{extra}end=1\n"
    (tmp_path / CFG).write_text(text)
    return text


def test_button_layout_defaults_to_nintendo_and_logs_the_map(persist_bin: Path, tmp_path: Path):
    # D2 (owner 2026-10-31): A confirms and B cancels by default.
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=1"]
    assert "dw3-controls: map loaded layout=nintendo source=default" in err
    assert "dw3-controls: map cross<-A circle<-X triangle<-B square<-Y" in err
    assert "dw3-options: defaults_gen=2 (new file)" in err
    assert not (tmp_path / CFG).exists()          # reading alone writes nothing


def test_button_layout_playstation_stays_when_the_file_says_so(persist_bin: Path, tmp_path: Path):
    seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=0"]
    assert "dw3-controls: map loaded layout=playstation source=cfg" in err
    assert "dw3-controls: map cross<-B circle<-A triangle<-X square<-Y" in err
    assert "migrate defaults_gen" not in err


def test_button_layout_confirm_toggles_and_is_saved_as_a_word(persist_bin: Path, tmp_path: Path):
    seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "open", "row", "9", "press", "layout")
    assert out == ["button_layout=1"]
    assert "button_layout=nintendo\n" in (tmp_path / CFG).read_text()
    assert "dw3-controls: layout changed playstation -> nintendo (screen)" in err
    assert "dw3-controls: map cross<-A circle<-X triangle<-B square<-Y" in err
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=1"]
    assert "dw3-controls: map loaded layout=nintendo source=cfg" in err
    out, err = session(persist_bin, tmp_path, "open", "row", "9", "left", "layout")
    assert out == ["button_layout=0"]
    assert "button_layout=playstation\n" in (tmp_path / CFG).read_text()


def test_button_layout_bad_values_fall_back_to_the_default(persist_bin: Path, tmp_path: Path):
    for bad in ("custom", "1", "Nintendo", ""):   # custom without a button_map is bad too
        (tmp_path / CFG).write_text(f"version=1\ndefaults_gen=2\nbutton_layout={bad}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "layout")
        assert out == ["button_layout=1"], bad    # D2: the default is Nintendo
        assert "dw3-controls: map invalid (" in err and "using default" in err, bad
        assert "dw3-controls: map loaded layout=nintendo source=default" in err, bad


def test_button_layout_keeps_the_other_rows_and_old_files(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nwidescreen=0\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "layout", "show")
    assert out == ["button_layout=1", "widescreen=0 fast_forward=2 restart_hotkey=0"]


def test_dw3_button_map_default_forces_playstation_for_one_run(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nbutton_layout=nintendo\nend=1\n")
    env = dict(os.environ, DW3_BUTTON_MAP="default")
    r = subprocess.run([str(persist_bin), str(tmp_path), "layout", "open", "row", "0", "press", "close"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert r.stdout.split()[0] == "button_layout=0"
    assert "dw3-controls: map loaded layout=playstation source=env" in r.stderr
    # an unrelated change does not write the forced value over the file's
    assert "button_layout=nintendo\n" in (tmp_path / CFG).read_text()
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=1"]


# --- P6.12 Customize buttons (docs/CONTROLS_BUNDLE.md) ------------------------

# raw words, a clear bit = pressed: Circle position (A) bit 13, R1 bit 11, Start bit 3
IDLE, SRC_A, SRC_R1, START = "FFFF", "DFFF", "F7FF", "FFF7"


def test_customize_assign_is_saved_as_custom_with_the_map(persist_bin: Path, tmp_path: Path):
    seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "open", "row", "11", "raw", IDLE, "press",   # opens Customize
                       "press",                                                          # row Cross waits
                       "raw", SRC_A, "tick", "map", "layout")
    assert out == ["map=1023456789 layout=2", "button_layout=2"]
    assert "dw3-port-options: group open customize (10 row(s), title)" in err
    assert "dw3-port-options: customize waiting for a button for Cross" in err
    assert "dw3-controls: assign cross<-A (circle<-B swapped in) [custom]" in err
    text = (tmp_path / CFG).read_text()
    assert "button_layout=custom\n" in text and "button_map=1023456789\n" in text
    out, err = session(persist_bin, tmp_path, "map", "layout")
    assert out == ["map=1023456789 layout=2", "button_layout=2"]
    assert "dw3-controls: map loaded layout=custom source=cfg" in err
    assert "dw3-controls: map cross<-A circle<-B triangle<-X square<-Y" in err


def test_customize_reset_restores_the_default_and_drops_the_map(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\ndefaults_gen=2\nbutton_layout=custom\nbutton_map=0123546789\nend=1\n")
    out, err = session(persist_bin, tmp_path, "map", "open", "row", "12", "press", "map", "layout")
    # D2: Reset to default puts the Nintendo layout back
    assert out == ["map=0123546789 layout=2", "map=1203456789 layout=1", "button_layout=1"]
    assert "dw3-controls: reset to default (layout=nintendo)" in err
    text = (tmp_path / CFG).read_text()
    assert "button_layout=nintendo\n" in text and "button_map" not in text


def test_customize_prompt_cancels_with_start_and_the_timeout(persist_bin: Path, tmp_path: Path):
    seed = seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "open", "row", "11", "raw", IDLE, "press", "press",
                       "raw", START, "tick", "map")
    assert out == ["map=0123456789 layout=0"]
    assert "customize Cross cancelled (Start / Select)" in err
    assert (tmp_path / CFG).read_text() == seed   # a cancel writes nothing
    out, err = session(persist_bin, tmp_path, "open", "row", "11", "raw", IDLE, "press", "press",
                       *(["tick"] * 305), "map")
    assert out == ["map=0123456789 layout=0"]
    assert "customize Cross cancelled (timeout)" in err


def test_customize_ignores_the_press_that_opened_the_prompt(persist_bin: Path, tmp_path: Path):
    # A is still down when the prompt opens (the Confirm press): it is not taken
    # until it has been let go. R1 then goes to the Cross row.
    seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "open", "row", "11", "raw", SRC_A, "press", "press",
                       "raw", SRC_A, "tick", "map", "raw", IDLE, "tick", "raw", SRC_R1, "tick", "map")
    assert out == ["map=0123456789 layout=0", "map=5123406789 layout=2"]
    assert "assign cross<-R (r1<-B swapped in) [custom]" in err


def test_customize_left_right_cycle_the_sources(persist_bin: Path, tmp_path: Path):
    seed_ps(tmp_path)
    out, err = session(persist_bin, tmp_path, "open", "row", "11", "press", "right", "map")
    assert out == ["map=1023456789 layout=2"]
    assert "dw3-controls: assign cross<-A (circle<-B swapped in) [custom]" in err
    assert "button_map=1023456789\n" in (tmp_path / CFG).read_text()


def test_custom_layout_cfg_must_carry_a_valid_map(persist_bin: Path, tmp_path: Path):
    for text in ("button_layout=custom\n", "button_layout=custom\nbutton_map=0123456780\n",
                 "button_layout=custom\nbutton_map=abc\n", "button_layout=custom\nbutton_map=012345678\n"):
        (tmp_path / CFG).write_text("version=1\ndefaults_gen=2\n" + text + "end=1\n")
        out, err = session(persist_bin, tmp_path, "map", "layout")
        assert out == ["map=1203456789 layout=1", "button_layout=1"], text   # D2: Nintendo
        assert "dw3-controls: map invalid (" in err and "using default" in err, text
    # a stale map beside a preset is ignored
    (tmp_path / CFG).write_text("version=1\nbutton_layout=nintendo\nbutton_map=0123546789\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "map")
    assert out == ["map=1203456789 layout=1"]


def test_dw3_button_map_default_keeps_the_custom_map_in_the_file(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nbutton_layout=custom\nbutton_map=0123546789\nend=1\n")
    env = dict(os.environ, DW3_BUTTON_MAP="default")
    r = subprocess.run([str(persist_bin), str(tmp_path), "map", "open", "row", "0", "press", "close"],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert r.stdout.split()[0] == "map=0123456789"
    text = (tmp_path / CFG).read_text()
    assert "button_layout=custom\n" in text and "button_map=0123546789\n" in text


def region_tree(tmp_path: Path, top: str = "") -> tuple[Path, Path]:
    """<tmp>/dw3 (the top-level folder) with the region folder <tmp>/dw3/eu."""
    base = tmp_path / "dw3"
    (base / "eu").mkdir(parents=True)
    if top:
        (base / CFG).write_text(top)
    return base, base / "eu"


def test_region_row_is_off_the_list_of_a_single_region_build(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "region")
    assert out == ["region_row=0"]          # registry slot exists, never listed (groups test)


def test_region_row_reads_the_top_level_file(region_bin: Path, tmp_path: Path):
    base, eu = region_tree(tmp_path, "region_default=us\nlast_region=eu\nend=1\n")
    out, err = session(region_bin, eu, "region")
    assert out == ["region_row=1"]
    assert f"dw3-options: region_default us read from {base}/{CFG}" in err
    base2, eu2 = region_tree(tmp_path / "b", "region_default=bogus\nend=1\n")
    out, _ = session(region_bin, eu2, "region")
    assert out == ["region_row=0"]          # an unreadable value is Ask


def test_region_row_confirm_cycles_and_writes_only_the_top_level_file(region_bin: Path, tmp_path: Path):
    base, eu = region_tree(tmp_path, "last_region=eu\nstage_sweep=0\nend=1\n")
    out, err = session(region_bin, eu, "open", "row", "10", "press", "region")
    assert out == ["region_row=1"]          # Ask -> US
    assert f"dw3-options: region_default us saved to {base}/{CFG} (takes effect at the next start)" in err
    top = (base / CFG).read_text()
    assert "region_default=us\n" in top and "last_region=eu\n" in top and "stage_sweep=0\n" in top
    assert top.rstrip().endswith("end=1")
    # the region folder gets no file from this row
    assert not (eu / CFG).exists()          # this row never creates the region file
    out, err = session(region_bin, eu, "open", "row", "10", "press", "region")
    assert out == ["region_row=2"]          # US -> EU
    assert "region_default=eu\n" in (base / CFG).read_text()
    out, err = session(region_bin, eu, "open", "row", "10", "press", "region")
    assert out == ["region_row=0"]          # EU -> Ask
    assert "region_default=ask\n" in (base / CFG).read_text()
    out, _ = session(region_bin, eu, "region")
    assert out == ["region_row=0"]


def test_region_row_never_overwrites_a_top_level_file_it_cannot_read(region_bin: Path, tmp_path: Path):
    base, eu = region_tree(tmp_path, "region_default=eu\nend=1\n")
    os.chmod(base / CFG, 0)
    try:
        out, err = session(region_bin, eu, "open", "row", "10", "press", "region")
        assert out == ["region_row=1"]      # the screen shows the choice
        assert "NOT saved" in err
    finally:
        os.chmod(base / CFG, stat.S_IRUSR | stat.S_IWUSR)
    assert (base / CFG).read_text() == "region_default=eu\nend=1\n"


# --- D2: defaults_gen, the one-shot migration of the defaults (docs/OPTIONS_REORG.md 6.1) ---

GLYPHS = "14"   # registry row of Switch glyphs
OLD_FILE = ("version=1\nwidescreen=0\nbutton_layout=playstation\nswitch_glyphs=0\n"
            "music_volume=7\nend=1\n")


def keys_of(text: str) -> dict:
    return dict(l.split("=", 1) for l in text.splitlines() if "=" in l and not l.startswith("#"))


def test_an_old_file_is_migrated_once_and_written_at_once(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(OLD_FILE)
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]
    assert ("dw3-options: migrate defaults_gen 1->2 button_layout playstation->nintendo "
            "switch_glyphs 0->1 (kept: none)") in err
    assert "NOT saved" not in err
    kv = keys_of((tmp_path / CFG).read_text())          # written before anything was changed
    assert (kv["button_layout"], kv["switch_glyphs"], kv["defaults_gen"]) == ("nintendo", "1", "2")
    assert kv["widescreen"] == "0" and kv["music_volume"] == "7"   # nothing else touched
    # the second start does not migrate again ...
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=1"] and "migrate defaults_gen" not in err and "nothing to flip" not in err
    # ... so a player who puts PlayStation back keeps it (it is a deliberate choice now)
    session(persist_bin, tmp_path, "open", "row", "9", "left")
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=0"] and "migrate defaults_gen" not in err
    assert keys_of((tmp_path / CFG).read_text())["button_layout"] == "playstation"


def test_a_custom_layout_is_kept_by_the_migration(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(
        "version=1\nbutton_layout=custom\nbutton_map=0123546789\nswitch_glyphs=0\nend=1\n")
    out, err = session(persist_bin, tmp_path, "map", "level", GLYPHS)
    assert out == ["map=0123546789 layout=2", f"level{GLYPHS}=1"]
    assert ("dw3-options: migrate defaults_gen 1->2 switch_glyphs 0->1 "
            "(kept: button_layout=custom)") in err
    kv = keys_of((tmp_path / CFG).read_text())
    assert (kv["button_layout"], kv["button_map"], kv["defaults_gen"]) == ("custom", "0123546789", "2")


def test_an_explicit_nintendo_and_glyphs_on_are_kept(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nbutton_layout=nintendo\nswitch_glyphs=1\nend=1\n")
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]
    assert "dw3-options: defaults_gen 1->2 nothing to flip" in err
    assert "migrate defaults_gen 1->2 button_layout" not in err
    assert keys_of((tmp_path / CFG).read_text())["defaults_gen"] == "2"   # the marker is stamped


def test_an_explicit_nintendo_with_glyphs_off_flips_only_the_glyphs(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nbutton_layout=nintendo\nswitch_glyphs=0\nend=1\n")
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]
    assert ("dw3-options: migrate defaults_gen 1->2 switch_glyphs 0->1 "
            "(kept: button_layout=nintendo)") in err


def test_a_missing_key_takes_the_new_default_without_a_flip(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\nwidescreen=0\nend=1\n")
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]
    assert "dw3-options: defaults_gen 1->2 nothing to flip" in err
    assert keys_of((tmp_path / CFG).read_text())["defaults_gen"] == "2"


def test_a_fresh_install_is_not_migrated_and_gets_the_marker_on_its_first_save(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]
    assert "dw3-options: defaults_gen=2 (new file)" in err
    assert "migrate defaults_gen" not in err
    assert not (tmp_path / CFG).exists()                # reading alone writes nothing
    session(persist_bin, tmp_path, "open", "row", "0", "press")
    kv = keys_of((tmp_path / CFG).read_text())
    assert (kv["defaults_gen"], kv["button_layout"], kv["switch_glyphs"]) == ("2", "nintendo", "1")


def test_a_current_file_is_never_touched(persist_bin: Path, tmp_path: Path):
    text = seed_ps(tmp_path, "switch_glyphs=0\n")
    out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    assert out == ["button_layout=0", f"level{GLYPHS}=0"]
    assert "migrate defaults_gen" not in err and "nothing to flip" not in err
    assert (tmp_path / CFG).read_text() == text


def test_a_newer_generation_is_kept(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\ndefaults_gen=3\nbutton_layout=playstation\nend=1\n")
    out, err = session(persist_bin, tmp_path, "layout", "open", "row", "0", "press")
    assert out == ["button_layout=0"] and "migrate defaults_gen" not in err
    assert keys_of((tmp_path / CFG).read_text())["defaults_gen"] == "3"   # not lowered


def test_a_bad_marker_counts_as_generation_one(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text("version=1\ndefaults_gen=soon\nbutton_layout=playstation\nend=1\n")
    out, err = session(persist_bin, tmp_path, "layout")
    assert out == ["button_layout=1"]
    assert "defaults_gen=soon is not valid" in err and "migrate defaults_gen 1->2" in err


@pytest.mark.skipif(os.geteuid() == 0, reason="root ignores directory permissions")
def test_a_failed_migration_write_is_logged_and_retried_at_the_next_start(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(OLD_FILE)
    tmp_path.chmod(stat.S_IRUSR | stat.S_IXUSR)          # the temp file cannot be created
    try:
        out, err = session(persist_bin, tmp_path, "layout", "level", GLYPHS)
    finally:
        tmp_path.chmod(stat.S_IRWXU)
    assert out == ["button_layout=1", f"level{GLYPHS}=1"]  # in force for the run, in memory
    assert "dw3-options: migrate defaults_gen NOT saved (" in err and "will retry" in err
    assert (tmp_path / CFG).read_text() == OLD_FILE        # the file is untouched
    out, err = session(persist_bin, tmp_path, "layout")    # the next start tries again
    assert out == ["button_layout=1"] and "NOT saved" not in err
    assert keys_of((tmp_path / CFG).read_text())["defaults_gen"] == "2"


def test_the_environment_pins_are_applied_after_the_migration_and_never_saved(persist_bin: Path, tmp_path: Path):
    (tmp_path / CFG).write_text(OLD_FILE)
    env = dict(os.environ, DW3_BUTTON_MAP="default", DW3_SWITCH_GLYPHS="0")
    r = subprocess.run([str(persist_bin), str(tmp_path), "layout", "level", GLYPHS],
                       capture_output=True, text=True, env=env)
    assert r.returncode == 0, r.stderr
    assert r.stdout.split() == ["button_layout=0", f"level{GLYPHS}=0"]     # the run is pinned
    kv = keys_of((tmp_path / CFG).read_text())
    assert (kv["button_layout"], kv["switch_glyphs"]) == ("nintendo", "1")  # the file is migrated


# --- D5: the Map scaler row (registry row 27, key map_scaler, FSR 1 by default) -------------

SCALER = "27"


def test_map_scaler_defaults_to_fsr1_and_is_a_word_in_the_file(persist_bin: Path, tmp_path: Path):
    out, _ = session(persist_bin, tmp_path, "level", SCALER)
    assert out == [f"level{SCALER}=3"]                                  # FSR 1
    session(persist_bin, tmp_path, "open", "row", SCALER, "press")      # 3 -> 4 (Bilinear)
    assert keys_of((tmp_path / CFG).read_text())["map_scaler"] == "bilinear"


def test_map_scaler_steps_through_the_five_values_and_wraps(persist_bin: Path, tmp_path: Path):
    words = []
    for _ in range(6):
        session(persist_bin, tmp_path, "open", "row", SCALER, "press")
        words.append(keys_of((tmp_path / CFG).read_text())["map_scaler"])
    assert words == ["bilinear", "nearest", "sharp", "lanczos2", "fsr1", "bilinear"]
    session(persist_bin, tmp_path, "open", "row", SCALER, "left")
    assert keys_of((tmp_path / CFG).read_text())["map_scaler"] == "fsr1"


@pytest.mark.parametrize("word,value", [("nearest", 0), ("sharp", 1), ("lanczos2", 2),
                                         ("fsr1", 3), ("bilinear", 4)])
def test_map_scaler_loads_each_word(persist_bin: Path, tmp_path: Path, word, value):
    (tmp_path / CFG).write_text(f"version=1\ndefaults_gen=2\nmap_scaler={word}\nend=1\n")
    out, _ = session(persist_bin, tmp_path, "level", SCALER)
    assert out == [f"level{SCALER}={value}"]


def test_map_scaler_bad_values_fall_back_to_fsr1(persist_bin: Path, tmp_path: Path):
    for bad in ("3", "FSR1", "fsr 1", ""):
        (tmp_path / CFG).write_text(f"version=1\ndefaults_gen=2\nmap_scaler={bad}\nend=1\n")
        out, err = session(persist_bin, tmp_path, "level", SCALER)
        assert out == [f"level{SCALER}=3"], bad
        assert f"map_scaler={bad} is not valid; using default fsr1" in err, bad
