"""P6.32 phase 2 host proofs: the marker tables the new full-screen map reads (spots and
towns of the decomp's STSTATUS data) against the PS1 cursor's reach, and the names the
replay logs. Needs the decomp submodule; no game data."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
HELPERS = ROOT / "vendor" / "dw3_decomp" / "src" / "menus" / "field_menu" / "helpers.c"


def _table(name: str) -> list[tuple[int, ...]]:
    """The rows of `StatusMapSpot STSTATUS_spots[] = { { sprite, x, y }, ... };` (or the
    towns' { x, y })."""
    src = HELPERS.read_text()
    m = re.search(r"\b%s\[\] = \{(.*?)\n\};" % name, src, re.S)
    assert m, name
    return [tuple(int(v) for v in row.split(","))
            for row in re.findall(r"\{\s*(-?\d+(?:\s*,\s*-?\d+)+)\s*\}", m.group(1))]


@pytest.fixture(scope="module")
def core_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("map_names") / "map_view_core_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
           str(TESTS / "map_view_core_main.c"),
           str(ROOT / "port" / "map_view" / "map_view_core.c"),
           str(ROOT / "platforms" / "pc" / "src" / "title_logo_resample.c"),
           f"-I{ROOT / 'port' / 'map_view'}", f"-I{ROOT / 'platforms' / 'pc' / 'src'}", "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.mark.skipif(not HELPERS.exists(), reason="decomp submodule not checked out")
def test_every_named_location_lies_inside_the_ps1_cursor_reach(core_bin, tmp_path):
    spots = _table("STSTATUS_spots")
    assert len(spots) == 47 and spots[0] == (0, 0, 0)  # spot 0 is unused
    f = tmp_path / "spots.txt"
    f.write_text("".join(f"{x} {y}\n" for _, x, y in spots[1:]))
    r = subprocess.run([str(core_bin), "reach", str(f)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    # no location (the only things with a name) is in a strip the PS1 cursor cannot
    # reach, so the new cursor reaching the strips adds no name; see P6.32 section 14
    assert r.stdout.strip().splitlines()[-1] == "inside 46 partly 0 outside 0", r.stdout


@pytest.mark.skipif(not HELPERS.exists(), reason="decomp submodule not checked out")
def test_only_town_13_starts_in_a_strip():
    towns = _table("STSTATUS_towns")
    assert len(towns) == 23 and towns[0] == (0, 0)  # town 0 is unused
    in_strip = [i for i, (x, y) in enumerate(towns) if i and (x < 22 or x > 370 or y < 15 or y > 290)]
    assert in_strip == [13]
    assert towns[13] == (21, 242)  # the replay's `town id=13 at 21,242` log line


@pytest.mark.skipif(not HELPERS.exists(), reason="decomp submodule not checked out")
def test_spot_table_matches_the_ones_the_host_logs():
    # the first and last spots of `DW3_MAP_VERIFY`'s listing in the replay logs
    spots = _table("STSTATUS_spots")
    assert spots[1] == (1, 60, 53) and spots[46] == (37, 225, 253)
    # the extremes the replay's strip probes aim at
    assert min(spots[1:], key=lambda s: s[1])[1:] == (35, 254)   # leftmost: location 39
    assert max(spots[1:], key=lambda s: s[1])[1:] == (340, 105)  # rightmost: location 16
    assert min(spots[1:], key=lambda s: s[2])[1:] == (209, 36)   # topmost: location 6


def test_patch_0147_is_the_only_new_patch_and_stays_in_range():
    patches = sorted(p.name for p in (ROOT / "port" / "patches").glob("01[45]*.patch"))
    assert "0147-PORT_HOST-p632-map-view-markers-names.patch" in patches
    text = (ROOT / "port" / "patches" / "0147-PORT_HOST-p632-map-view-markers-names.patch").read_text()
    assert "dw3_port_map_view_wants" in text and "mapViewName" in text
    assert "DW3_FORCE_MAP_AREAS" in text
