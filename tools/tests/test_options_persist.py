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
                    "music_volume", "sfx_volume", "end"]
