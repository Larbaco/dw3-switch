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
def persist_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("optpersist") / "options_persist_test",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST"])


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
    session(persist_bin, tmp_path, "env", "0", "0", "open", "press")
    assert "widescreen=1\n" in (tmp_path / CFG).read_text()  # was forced 0 -> 1
    session(persist_bin, tmp_path, "env", "0", "1", "open", "press")
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
                    "end"]


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
    _, err = session(persist_bin, tmp_path, "open", "press")
    assert sounds(err) == ["select 10"]
    _, err = session(persist_bin, tmp_path, "open", "down", "press")
    assert sounds(err) == ["cursor 10", "select 10"]
    _, err = session(persist_bin, tmp_path, "open", "row", "5", "press")
    assert sounds(err)[-1] == "select 10" and sounds(err).count("cursor 10") == 5
    # Left / Right on a stepped row change the value, so they play it too.
    _, err = session(persist_bin, tmp_path, "open", "row", "6", "right", "left")
    assert sounds(err)[-2:] == ["select 10", "select 10"]


def test_no_sound_when_nothing_changes(persist_bin: Path, tmp_path: Path):
    # Up on the first row, Down on the last, and Left / Right on a row that
    # does not take them or that is already at its end: silent, as the game's
    # own menus are at the end of a list.
    _, err = session(persist_bin, tmp_path, "open", "up", "left", "right")
    assert sounds(err) == []
    _, err = session(persist_bin, tmp_path, "open", "row", "3", "right")
    assert sounds(err) == ["cursor 10"] * 3          # Music volume is already at 10
    _, err = session(persist_bin, tmp_path, "open", "row", "6", "left")
    assert sounds(err) == ["cursor 10"] * 6          # Text speed is already at Normal
    _, err = session(persist_bin, tmp_path, "open", "row", "6", "down")
    assert sounds(err) == ["cursor 10"] * 6          # the last row
    # The plain open and close calls make no sound; the screen plays Select
    # for the opening from the title menu (decomp patch 0044), not here.
    _, err = session(persist_bin, tmp_path, "open", "close")
    assert sounds(err) == []


def test_cancel_sound_on_back(persist_bin: Path, tmp_path: Path):
    out, err = session(persist_bin, tmp_path, "open", "back")
    assert sounds(err) == ["cancel 10"]
    assert "dw3-port-options: close" in err


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
