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
                 OPTIONS / "options_cfg.c", defs=["-DDW3_PORT_OPTIONS_TEST"])


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
