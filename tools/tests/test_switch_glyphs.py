"""P10.6 Switch glyphs: the pure part of port/options/switch_glyphs.c (art, role
maps, 4 bpp packing, the plan per button map), the generated art and the PNG
files in assets/glyphs/."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
sys.path.insert(0, str(ROOT / "tools"))
import check_switch_glyphs as C  # noqa: E402
import generate_switch_glyphs as G  # noqa: E402


@pytest.fixture(scope="module")
def sg_bin(tmp_path_factory) -> Path:
    d = tmp_path_factory.mktemp("switch_glyphs")
    out = d / "switch_glyphs_test"
    inc = [f"-I{ROOT / 'port' / 'include'}", f"-I{ROOT / 'port' / 'options'}",
           f"-I{ROOT / 'port' / 'mods'}"]
    # button_map.c is built as it is in the button map test; the glyph file with
    # the host part left out (DW3_GLYPHS_TEST).
    objs = []
    for src, defs in ((ROOT / "port" / "options" / "button_map.c", ["-DDW3_BUTTON_MAP_TEST"]),
                      (ROOT / "port" / "options" / "switch_glyphs.c", ["-DDW3_GLYPHS_TEST"]),
                      (ROOT / "port" / "mods" / "mod_pack.c", ["-std=gnu11"]),
                      (ROOT / "port" / "mods" / "png_stb.c", ["-std=gnu11"]),
                      (TESTS / "switch_glyphs_main.c", ["-DDW3_GLYPHS_TEST"])):
        obj = d / (src.stem + ".o")
        std = [] if any(x.startswith("-std") for x in defs) else ["-std=gnu89"]
        r = subprocess.run(["gcc", *std, "-Wall", "-Wextra", "-Werror", "-O2",
                            "-Wno-unused-function", *defs, *inc, "-c", "-o", str(obj), str(src)],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stderr + r.stdout
        objs.append(str(obj))
    r = subprocess.run(["gcc", "-o", str(out), *objs], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_switch_glyphs_unit(sg_bin: Path):
    r = subprocess.run([str(sg_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: switch glyphs" in r.stdout


def test_compiled_art_is_what_the_generator_draws(sg_bin: Path):
    out = subprocess.run([str(sg_bin), "dump"], capture_output=True, text=True).stdout.split()
    want = []
    for n in G.NAMES:
        want.append(n.upper())
        want.extend("".join(str(v) for v in row) for row in G.glyph(n))
    assert out == want


def test_assets_check():
    assert C.check() == []


def test_check_catches_a_bad_file(tmp_path, monkeypatch):
    """A glyph of the wrong size and a PROVENANCE without the statements fail."""
    bad = tmp_path / "glyphs"
    bad.mkdir()
    for p in G.OUT_DIR.iterdir():
        (bad / p.name).write_bytes(p.read_bytes())
    wide = bytearray(G.png_bytes(G.glyph("a")))
    # IHDR width 12 -> 13 (the CRC no longer matches either: both are caught)
    wide[19] = 13
    (bad / "a.png").write_bytes(bytes(wide))
    (bad / "PROVENANCE.md").write_text("nothing useful here\n")
    monkeypatch.setattr(G, "OUT_DIR", bad)
    errs = C.check()
    assert any("a.png" in e for e in errs)
    assert any("PROVENANCE.md does not state" in e for e in errs)


def test_png_roundtrip():
    for n in G.NAMES:
        w, h, depth, ctype, inter, pal, trns, rows = C.read_png(G.png_bytes(G.glyph(n)))
        assert (w, h, depth, ctype, inter) == (12, 12, 8, 3, 0)
        assert rows == G.glyph(n)
        assert pal == G.ROLE_RGB and trns[0] == 0


def test_png_files_load_and_equal_the_art(sg_bin: Path):
    """The loader of the override folder reads the committed PNGs (what a
    player's own glyphs are read with) and gets exactly the compiled-in art."""
    r = subprocess.run([str(sg_bin), "png", str(G.OUT_DIR)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: png files equal the art" in r.stdout


def test_png_override_errors_are_named(sg_bin: Path, tmp_path: Path):
    """A missing file, a file of another size and a glyph in other colours are
    refused with a reason (the port keeps the bundled glyph and logs it)."""
    PIL = pytest.importorskip("PIL.Image")
    for p in G.OUT_DIR.glob("*.png"):
        (tmp_path / p.name).write_bytes(p.read_bytes())
    (tmp_path / "zr.png").unlink()
    PIL.new("RGBA", (13, 12), (255, 255, 255, 255)).save(tmp_path / "x.png")
    PIL.new("RGBA", (12, 12), (10, 200, 10, 255)).save(tmp_path / "y.png")
    r = subprocess.run([str(sg_bin), "png", str(tmp_path)], capture_output=True, text=True)
    assert r.returncode == 1
    assert "png zr: cannot open" in r.stdout
    assert "png x: 13x12, not 12x12" in r.stdout
    assert "png y: pixel 0,0 is 10,200,10, not a role colour" in r.stdout
    assert "png a:" not in r.stdout
