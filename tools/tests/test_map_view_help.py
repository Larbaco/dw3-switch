"""P6.32 phase 7: the map view's help line (port/map_view/map_view_help.c): the names per
layout, custom map and binding on the PC and on the Switch (text and glyphs), the words in the
five languages, and that every line fits the 640 px bar. Plain C, no game data."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


@pytest.fixture(scope="module")
def help_bin(tmp_path_factory) -> Path:
    d = tmp_path_factory.mktemp("map_view_help")
    out = d / "map_view_help_test"
    inc = [f"-I{ROOT / 'port' / 'include'}", f"-I{ROOT / 'port' / 'options'}",
           f"-I{ROOT / 'port' / 'mods'}", f"-I{ROOT / 'port' / 'map_view'}", f"-I{ROOT / 'platforms' / 'pc' / 'src'}"]
    objs = []
    # button_map.c as in the button map test, the glyph file with its host part left out, the
    # help file and the core it reads the bindings from, and the test (all as gnu89 like the port)
    for src, defs in ((ROOT / "port" / "options" / "button_map.c", ["-DDW3_BUTTON_MAP_TEST"]),
                      (ROOT / "port" / "options" / "switch_glyphs.c", ["-DDW3_GLYPHS_TEST"]),
                      (ROOT / "port" / "mods" / "mod_pack.c", ["-std=gnu11"]),
                      (ROOT / "port" / "mods" / "png_stb.c", ["-std=gnu11"]),
                      (ROOT / "port" / "map_view" / "map_view_core.c", []),
                      (ROOT / "platforms" / "pc" / "src" / "title_logo_resample.c", ["-std=gnu11"]),
                      (ROOT / "port" / "map_view" / "map_view_help.c", []),
                      (TESTS / "map_view_help_main.c", ["-std=gnu11", "-DDW3_GLYPHS_TEST"])):
        obj = d / (src.stem + ".o")
        std = [] if any(x.startswith("-std") for x in defs) else ["-std=gnu89"]
        r = subprocess.run(["gcc", *std, "-Wall", "-Wextra", "-Werror", "-O2",
                            "-Wno-unused-function", "-Wno-long-long", *defs, *inc, "-c", "-o", str(obj), str(src)],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stderr + r.stdout
        objs.append(str(obj))
    r = subprocess.run(["gcc", "-o", str(out), *objs, "-lm"], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_map_view_help(help_bin: Path):
    r = subprocess.run([str(help_bin)], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: map view help" in r.stdout
    print(r.stdout)
