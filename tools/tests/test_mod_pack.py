"""Host proofs of the MOD loader (port/mods/mod_pack.c, docs/MOD_MAP_UPSCALE.md):
manifest parsing, pack discovery, and the map / sprite loads with a valid pack,
a missing file, a wrong-sized file, a truncated PNG, a newer manifest format and
a pack for another region. The fixtures are synthetic gradients (no game art),
written with Pillow; the extractor's helpers are checked on synthetic data too."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path

import pytest

pytest.importorskip("PIL")
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"


def gradient(w: int, h: int, alpha: bool = False) -> Image.Image:
    img = Image.new("RGBA", (w, h))
    img.putdata([(x * 255 // max(w - 1, 1), y * 255 // max(h - 1, 1), 128,
                  128 if alpha else 255) for y in range(h) for x in range(w)])
    return img


def manifest(**over) -> dict:
    m = {"format": 1, "id": "synthetic", "name": "Synthetic", "version": "0.0.1",
         "regions": ["us"], "map": {"scales": [2, 4], "filter": "nearest", "reveal": True}}
    m.update(over)
    return m


def make_pack(root: Path, name: str, man: dict | str | None, *, maps=(), sprites=()) -> Path:
    pack = root / name
    (pack / "map" / "reveal" / "us_h0").mkdir(parents=True)
    if man is not None:
        (pack / "mod.json").write_text(man if isinstance(man, str) else json.dumps(man))
    for half, scale, size in maps:
        gradient(*size).save(pack / "map" / f"map_us_h{half}@{scale}x.png")
    for kind, fid, scale, size in sprites:
        gradient(*size, alpha=True).save(pack / "map" / "reveal" / "us_h0" / f"{kind}_{fid}@{scale}x.png")
    return pack


@pytest.fixture(scope="module")
def mod_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("mod_pack") / "mod_pack_test"
    cmd = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
        str(TESTS / "mod_pack_main.c"),
        str(ROOT / "port" / "mods" / "mod_pack.c"),
        str(ROOT / "port" / "mods" / "png_stb.c"),
        f"-I{ROOT / 'port' / 'mods'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def make_shared_pack(root: Path) -> None:
    """P51 item 3: one pack for both regions; the EU has its own h1 map and its
    own towns_5 sprite, which must win over the shared files."""
    pack = root / "shared"
    (pack / "map" / "reveal" / "h0").mkdir(parents=True)
    (pack / "map" / "reveal" / "eu_h0").mkdir(parents=True)
    (pack / "mod.json").write_text(json.dumps(manifest(id="shared", regions=["us", "eu"])))
    for half in (0, 1):
        gradient(32, 24).save(pack / "map" / f"map_h{half}@4x.png")
    gradient(32, 24).save(pack / "map" / "map_eu_h1@4x.png")
    gradient(12, 8, alpha=True).save(pack / "map" / "reveal" / "h0" / "towns_5@4x.png")
    gradient(12, 8, alpha=True).save(pack / "map" / "reveal" / "eu_h0" / "towns_5@4x.png")


def test_mod_pack(mod_bin: Path, tmp_path: Path):
    make_shared_pack(tmp_path / "shared_root")
    good = tmp_path / "good_root"
    make_pack(good, "good", manifest(), maps=[(0, 4, (32, 24)), (0, 2, (16, 12)),
                                              (1, 4, (32, 24)), (1, 2, (16, 12))],
              sprites=[("towns", 5, 4, (12, 8))])
    bad = tmp_path / "bad_root"
    make_pack(bad, "badsize", manifest(map={"scales": [4]}), maps=[(0, 4, (30, 24))])
    make_pack(bad, "newformat", manifest(format=99), maps=[(0, 4, (32, 24))])
    make_pack(bad, "junk", "format = 1", maps=[(0, 4, (32, 24))])
    make_pack(bad, "eu_only", manifest(regions=["eu"]), maps=[(0, 4, (32, 24))])
    trunc = make_pack(bad, "truncated", manifest(map={"scales": [4]}), maps=[(0, 4, (32, 24))])
    png = trunc / "map" / "map_us_h0@4x.png"
    png.write_bytes(png.read_bytes()[:40])
    r = subprocess.run([str(mod_bin), str(tmp_path)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: mod pack loader" in r.stdout


def test_mod_pack_compiles_as_c90(tmp_path: Path):
    """The port library is built as gnu89: the loader must be valid there."""
    for src in ("mod_pack.c", "png_stb.c"):
        cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
               "-c", "-o", str(tmp_path / (src + ".o")), str(ROOT / "port" / "mods" / src),
               f"-I{ROOT / 'port' / 'mods'}"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert r.returncode == 0, r.stderr + r.stdout


def test_extract_helpers_bleed_and_pad():
    """The extractor bleeds sprite colour into transparent texels and pads by
    replication (synthetic sprite)."""
    import importlib.util

    spec = importlib.util.spec_from_file_location("extract_map_assets",
                                                  ROOT / "tools" / "mods" / "extract_map_assets.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    img = Image.new("RGBA", (4, 3), (0, 0, 0, 0))
    img.putpixel((1, 1), (200, 10, 20, 255))
    rgb = mod.bleed(img)
    assert rgb.getpixel((3, 2)) == (200, 10, 20)  # the transparent texels took the colour
    assert rgb.getpixel((0, 0)) == (200, 10, 20)
    padded = mod.pad_replicate(rgb, 2)
    assert padded.size == (8, 7)
    assert padded.getpixel((0, 0)) == padded.getpixel((2, 2))
    assert padded.getpixel((7, 6)) == padded.getpixel((5, 4))
