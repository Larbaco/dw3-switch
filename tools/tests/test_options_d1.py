"""D1 (docs/OPTIONS_REORG.md): the rows that were file-only keys, the Map buttons
screen, the stage sweep's Start over rewrite and the cfg key budget."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
OPTIONS = ROOT / "port" / "options"
CFG = "port_options.cfg"

# registry row -> (cfg key, default)
MENUS = {16: "items_equip_from_items", 17: "items_equip_stays", 18: "items_bag_view",
         19: "items_use_stays", 20: "items_partner_switch", 27: "menu_instant_exit"}
SHOW_VERSION, MAP_MOD, SAVE_STORE, SWEEP, FRAMES, MAP_BUTTONS = 21, 22, 23, 24, 25, 26


def build(out: Path, *sources: Path, defs=()) -> Path:
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", *defs, "-o", str(out),
           *map(str, sources), f"-I{ROOT / 'port' / 'include'}", f"-I{ROOT / 'port' / 'map_view'}",
           f"-I{ROOT / 'platforms' / 'pc' / 'src'}", "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def plain_bin(tmp_path_factory) -> Path:
    """The registry with no Yes / No window (a build without the native overlay)."""
    return build(tmp_path_factory.mktemp("d1plain") / "t",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c", OPTIONS / "button_map.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST"])


@pytest.fixture(scope="module")
def ask_bin(tmp_path_factory) -> Path:
    """The registry with a fake Yes / No window the harness answers."""
    return build(tmp_path_factory.mktemp("d1ask") / "t",
                 TESTS / "options_persist_main.c", OPTIONS / "port_options.c", OPTIONS / "button_map.c",
                 OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST", "-DDW3_ASK_FAKE"])


@pytest.fixture(scope="module")
def mapbtn_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("d1mapbtn") / "t",
                 TESTS / "map_buttons_main.c", OPTIONS / "map_buttons.c", OPTIONS / "port_options.c",
                 OPTIONS / "button_map.c", OPTIONS / "options_cfg.c", ROOT / "port" / "audio" / "volume.c",
                 ROOT / "port" / "map_view" / "map_view_core.c",
                 ROOT / "platforms" / "pc" / "src" / "title_logo_resample.c",
                 defs=["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST", "-DDW3_MAPBTN_TEST"])


def session(binary: Path, cfgdir: Path, *steps: str):
    r = subprocess.run([str(binary), str(cfgdir), *steps], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip().splitlines(), r.stderr


def cfg_text(d: Path) -> str:
    return (d / CFG).read_text()


# --- the new rows: defaults, files, switches --------------------------------------

def test_new_rows_default_to_the_values_of_the_file_only_keys(plain_bin, tmp_path):
    steps = []
    for row in (*MENUS, SHOW_VERSION, MAP_MOD, SAVE_STORE, SWEEP, FRAMES):
        steps += ["level", str(row)]
    out, _ = session(plain_bin, tmp_path, *steps)
    assert out == ["level16=1", "level17=1", "level18=1", "level19=1", "level20=1", "level27=1",
                   "level21=1", "level22=1", "level23=0", "level24=0", "level25=90"]


def test_a_file_written_before_the_rows_existed_loads_into_them(plain_bin, tmp_path):
    (tmp_path / CFG).write_text(
        "version=1\nitems_bag_view=0\nitems_equip_stays=0\nshow_version=0\nmap_mod=0\n"
        "save_store=1\nstage_sweep=2\nstage_sweep_frames=300\nend=1\n")
    steps = []
    for row in (17, 18, SHOW_VERSION, MAP_MOD, SAVE_STORE, SWEEP, FRAMES):
        steps += ["level", str(row)]
    out, _ = session(plain_bin, tmp_path, *steps)
    assert out == ["level17=0", "level18=0", "level21=0", "level22=0", "level23=1",
                   "level24=2", "level25=300"]


def test_bad_values_fall_back_to_the_defaults(plain_bin, tmp_path):
    (tmp_path / CFG).write_text(
        "version=1\nitems_use_stays=7\nstage_sweep=3\nstage_sweep_frames=5\nsave_store=yes\nend=1\n")
    out, err = session(plain_bin, tmp_path, "level", "19", "level", "24", "level", "25", "level", "23")
    assert out == ["level19=1", "level24=0", "level25=90", "level23=0"]
    assert "items_use_stays=7 is not valid; using default 1" in err
    assert "stage_sweep=3 is not valid; using default 0" in err
    assert "stage_sweep_frames=5 is not valid; using default 90" in err


@pytest.mark.parametrize("row", [*MENUS, SHOW_VERSION, MAP_MOD])
def test_a_switch_row_toggles_persists_and_is_seen_by_the_hook(plain_bin, tmp_path, row):
    key = {**MENUS, SHOW_VERSION: "show_version", MAP_MOD: "map_mod"}[row]
    out, _ = session(plain_bin, tmp_path, "open", "row", str(row), "press", "fileint", key, "1")
    assert out == [f"{key}=0"]                    # the hook (file_int) sees it at once
    assert f"{key}=0\n" in cfg_text(tmp_path)
    out, _ = session(plain_bin, tmp_path, "level", str(row), "fileint", key, "1")
    assert out == [f"level{row}=0", f"{key}=0"]   # and the next start
    session(plain_bin, tmp_path, "open", "row", str(row), "press")
    assert f"{key}=1\n" in cfg_text(tmp_path)


def test_left_and_right_do_nothing_on_a_switch_row(plain_bin, tmp_path):
    out, _ = session(plain_bin, tmp_path, "open", "row", "16", "left", "right", "level", "16")
    assert out == ["level16=1"]


def test_a_row_is_seen_by_the_hook_without_a_file(plain_bin, tmp_path):
    out, _ = session(plain_bin, tmp_path, "noinit", "open", "row", "18", "press", "fileint",
                     "items_bag_view", "1")
    assert out == ["items_bag_view=0"]


# --- Shared saves: a Yes / No window ------------------------------------------------

def test_shared_saves_asks_first_and_stays_off_on_no(ask_bin, tmp_path):
    out, err = session(ask_bin, tmp_path, "open", "row", str(SAVE_STORE), "press", "askstate",
                       "level", str(SAVE_STORE), "askno", "level", str(SAVE_STORE))
    assert out == ["window=open what=1", "level23=0", "level23=0"]
    assert "save_store 0->1 declined" in err
    assert "save_store=1" not in (cfg_text(tmp_path) if (tmp_path / CFG).exists() else "")


def test_shared_saves_turns_on_with_yes_and_logs_it(ask_bin, tmp_path):
    out, err = session(ask_bin, tmp_path, "open", "row", str(SAVE_STORE), "press", "level",
                       str(SAVE_STORE), "askyes", "level", str(SAVE_STORE))
    assert out == ["level23=0", "level23=1"]       # unchanged until the answer
    assert "dw3-options: save_store 0->1 (confirmed, next start)" in err
    assert "save_store=1\n" in cfg_text(tmp_path)
    # Turning it OFF needs no window.
    out, err = session(ask_bin, tmp_path, "open", "row", str(SAVE_STORE), "press", "askstate",
                       "level", str(SAVE_STORE))
    assert out == ["window=closed what=0", "level23=0"]
    assert "save_store=0\n" in cfg_text(tmp_path)


def test_without_a_window_shared_saves_applies_at_once_and_says_so(plain_bin, tmp_path):
    _, err = session(plain_bin, tmp_path, "open", "row", str(SAVE_STORE), "press")
    assert "save_store 0->1 (no window in this build, applied, next start)" in err
    assert "save_store=1\n" in cfg_text(tmp_path)


def test_the_pin_of_the_environment_is_not_written(ask_bin, tmp_path):
    import os
    r = subprocess.run([str(ask_bin), str(tmp_path), "level", str(SAVE_STORE), "open", "row", "16",
                        "press"], capture_output=True, text=True, env={**os.environ, "DW3_SAVE_STORE": "1"})
    assert r.stdout.splitlines()[0] == "level23=1"
    assert "save_store=0\n" in cfg_text(tmp_path)   # an unrelated change keeps the file's value


# --- Stage sweep and Sweep frames ------------------------------------------------

def test_stage_sweep_steps_off_run_start_over_and_asks_for_the_last_two(ask_bin, tmp_path):
    out, err = session(ask_bin, tmp_path, "open", "row", str(SWEEP), "press", "askstate", "askyes",
                       "level", str(SWEEP))
    assert out == ["window=open what=2", "level24=1"]
    assert "stage_sweep 0->1 (confirmed, next start)" in err
    out, err = session(ask_bin, tmp_path, "open", "row", str(SWEEP), "right", "askyes", "level",
                       str(SWEEP))
    assert out == ["level24=2"]
    assert "stage_sweep 1->2 (confirmed, next start)" in err
    # Confirm wraps from Start over to OFF with no window.
    out, _ = session(ask_bin, tmp_path, "open", "row", str(SWEEP), "press", "askstate", "level", str(SWEEP))
    assert out == ["window=closed what=0", "level24=0"]
    assert "stage_sweep=0\n" in cfg_text(tmp_path)
    # A No keeps it where it was.
    out, _ = session(ask_bin, tmp_path, "open", "row", str(SWEEP), "right", "askno", "level", str(SWEEP))
    assert out == ["level24=0"]


def test_sweep_frames_steps_clamp_and_persist(plain_bin, tmp_path):
    out, _ = session(plain_bin, tmp_path, "open", "row", str(FRAMES), "right", "level", str(FRAMES),
                     "left", "left", "level", str(FRAMES), "r1", "level", str(FRAMES), "l1", "l1",
                     "level", str(FRAMES))
    assert out == ["level25=120", "level25=60", "level25=360", "level25=30"]
    out, _ = session(plain_bin, tmp_path, "open", "row", str(FRAMES), "left", "left", "left", "left",
                     "level", str(FRAMES))
    assert out == ["level25=30"]                   # the floor
    assert "stage_sweep_frames=30\n" in cfg_text(tmp_path)
    out, _ = session(plain_bin, tmp_path, "open", "row", str(FRAMES), *["r1"] * 20, "level", str(FRAMES))
    assert out == ["level25=3600"]                 # the ceiling
    out, _ = session(plain_bin, tmp_path, "open", "row", str(FRAMES), "press", "level", str(FRAMES))
    assert out == ["level25=30"]                   # Confirm steps up and wraps to the floor


def test_start_over_is_rewritten_to_run_once_it_began(plain_bin, tmp_path):
    """The sweep forgets its progress at stage_sweep=2. If the key stayed 2, every start
    would clear it again and the sweep would never get past its first stage after a
    crash: the first tick rewrites 2 to 1 (dw3_port_options_sweep_started)."""
    (tmp_path / CFG).write_text("version=1\nstage_sweep=2\nstage_sweep_frames=60\nend=1\n")
    out, err = session(plain_bin, tmp_path, "level", str(SWEEP), "sweepstarted", "level", str(SWEEP))
    assert out == ["level24=2", "level24=1"]
    assert "dw3-options: stage_sweep 2->1" in err
    assert "stage_sweep=1\n" in cfg_text(tmp_path)
    assert "stage_sweep_frames=60\n" in cfg_text(tmp_path)   # the rest is kept
    # The next boot continues the run instead of starting over.
    out, err = session(plain_bin, tmp_path, "level", str(SWEEP), "sweepstarted")
    assert out == ["level24=1"]
    assert "stage_sweep 2->1" not in err
    # Run and OFF are left alone.
    (tmp_path / CFG).write_text("version=1\nstage_sweep=0\nend=1\n")
    session(plain_bin, tmp_path, "sweepstarted")
    assert "stage_sweep=0\n" in cfg_text(tmp_path)


def test_start_over_pinned_by_the_environment_is_not_written(plain_bin, tmp_path):
    import os
    (tmp_path / CFG).write_text("version=1\nstage_sweep=0\nend=1\n")
    r = subprocess.run([str(plain_bin), str(tmp_path), "sweepstarted"], capture_output=True, text=True,
                       env={**os.environ, "DW3_STAGE_SWEEP": "2"})
    assert r.returncode == 0
    assert "stage_sweep=0\n" in cfg_text(tmp_path)


# --- the Map buttons screen ------------------------------------------------------

def test_map_buttons_logic(mapbtn_bin, tmp_path):
    r = subprocess.run([str(mapbtn_bin), str(tmp_path)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: map buttons" in r.stdout
    text = cfg_text(tmp_path)
    for key in ("map_view_open", "map_view_zoom_out", "map_view_zoom_in", "map_view_centre",
                "map_view_scaler_prev", "map_view_scaler_next", "map_view_field_open"):
        assert f"{key}=" in text


def test_map_buttons_screen_opens_from_controls_and_goes_back(plain_bin, tmp_path):
    # Controls: Button layout, Customize, Map buttons, Reset, Restart (the PC has no glyphs row).
    out, err = session(plain_bin, tmp_path, "open", "nav", "26", "press", "back")
    assert "group open mapbtn (8 row(s), title)" in err
    assert "group close mapbtn (back to controls)" in err


def test_store_str_writes_and_bumps_the_generation(plain_bin, tmp_path):
    out, _ = session(plain_bin, tmp_path, "gen", "storestr", "map_view_open", "r2", "gen",
                     "fileval", "map_view_open")
    assert out == ["gen=0", "storestr rc=0", "gen=1", "map_view_open=r2"]
    assert "map_view_open=r2\n" in cfg_text(tmp_path)
    out, _ = session(plain_bin, tmp_path, "noinit", "storestr", "map_view_open", "r2", "fileval",
                     "map_view_open")
    assert out == ["storestr rc=-1", "map_view_open=(none)"]   # session-only: nothing to write


# --- the key budget --------------------------------------------------------------

# Keys of the file that are not registry rows (docs/OPTIONS_REORG.md section 1.2). The
# markers defaults_gen (D2) and options_global (D3) and the two top-level keys the global
# file of D3 will hold are counted already, so the next row cannot overflow silently.
EXTRA_KEYS = ["button_map", "map_view_open", "map_view_zoom_out", "map_view_zoom_in",
              "map_view_centre", "map_view_scaler_prev", "map_view_scaler_next",
              "map_view_field_open", "cheat_enabled", "cheat_instant_win", "defaults_gen",
              "options_global", "region_default", "last_region"]


def test_the_cfg_holds_every_key_within_the_budget(plain_bin, tmp_path):
    """Write a file with every key the port keeps and read it back: it must fit the
    64 entries and the 4096 bytes of options_cfg (port_options_cfg.h), with room to
    spare, so a new row that would overflow fails here and not on the player's SD."""
    out, _ = session(plain_bin, tmp_path, "keys")
    registry = out
    assert len(registry) == len(set(registry)), "a registry key is used twice"
    assert not set(registry) & set(EXTRA_KEYS)
    # button_map only exists while the layout is custom (po_save drops it otherwise).
    (tmp_path / CFG).write_text("version=1\nbutton_layout=custom\nbutton_map=0123456789\nend=1\n")
    steps = ["open", "row", "8", "press"]                       # one change writes every row
    for key in EXTRA_KEYS:
        if key != "button_map":
            steps += ["storestr", key, "select"]
    out, _ = session(plain_bin, tmp_path, *steps)
    assert out == ["storestr rc=0"] * (len(EXTRA_KEYS) - 1)     # none was refused (table full)
    text = cfg_text(tmp_path)
    keys = [l.split("=")[0] for l in text.splitlines() if "=" in l and not l.startswith("#")]
    entries = [k for k in keys if k not in ("version", "end")]
    assert set(registry + EXTRA_KEYS) <= set(entries)
    assert len(entries) <= 64
    assert len(text.encode()) <= 4096
    spare = 64 - len(entries)
    assert spare >= 6, f"only {spare} cfg entries left; raise DW3_OPTCFG_MAX_ENTRIES before the next row"
    # And it reads back: a new process loads the whole file.
    out, err = session(plain_bin, tmp_path, "fileval", "map_view_field_open", "level", "8")
    assert out == ["map_view_field_open=select", "level8=1"]
    assert "ignored" not in err
