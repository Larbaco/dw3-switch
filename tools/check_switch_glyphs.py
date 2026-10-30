#!/usr/bin/env python3
"""P10.6: check assets/glyphs/ (the Switch button glyphs).

  python3 tools/check_switch_glyphs.py        exit 0 when every check passes

Checks, with the standard library only:
  * the eight files a.png b.png x.png y.png l.png r.png zl.png zr.png exist, and
    nothing else but LICENSE and PROVENANCE.md is in the folder;
  * each PNG is 12x12, 8-bit palette (colour type 3), non-interlaced, with the
    five fixed role colours of tools/generate_switch_glyphs.py in the palette
    (index 0 transparent) and only indices 0..4 in the pixels;
  * each glyph has a rim, a body and a letter, and the letter does not touch the
    rim sideways (so it stays readable);
  * the PNGs and port/options/switch_glyph_art.inc are what the generator draws;
  * LICENSE says CC0 and PROVENANCE.md carries the statements AGENTS.md asks for
    (no extracted or derived game asset, no tracing / img2img / upscaling).
"""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import generate_switch_glyphs as G  # noqa: E402

ROOT = G.ROOT
PROVENANCE_WORDS = ["no extracted or derived game asset", "traced", "img2img",
                    "upscaled", "no nintendo or console asset"]


def read_png(data: bytes):
    """(width, height, bit depth, colour type, interlace, palette, trns, rows)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    chunks = {}
    idat = b""
    while pos < len(data):
        n, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack(">I", data[pos + 8 + n:pos + 12 + n])[0]
        if zlib.crc32(tag + body) & 0xFFFFFFFF != crc:
            raise ValueError(f"bad CRC in {tag!r}")
        if tag == b"IDAT":
            idat += body
        else:
            chunks[tag] = body
        pos += 12 + n
    w, h, depth, ctype, _comp, _filt, inter = struct.unpack(">IIBBBBB", chunks[b"IHDR"])
    palette = [tuple(chunks[b"PLTE"][i:i + 3]) for i in range(0, len(chunks.get(b"PLTE", b"")), 3)]
    raw = zlib.decompress(idat)
    if depth != 8 or ctype != 3 or inter != 0:
        return w, h, depth, ctype, inter, palette, chunks.get(b"tRNS", b""), None
    stride = w
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(h):
        f = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            a = line[i - 1] if i else 0
            b = prev[i]
            c = prev[i - 1] if i else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[i] = (line[i] + pr) & 255
        rows.append(list(line))
        prev = line
    return w, h, depth, ctype, inter, palette, chunks.get(b"tRNS", b""), rows


def check() -> list[str]:
    errs: list[str] = []
    d = G.OUT_DIR
    want = {f"{n}.png" for n in G.NAMES} | {"LICENSE", "PROVENANCE.md"}
    have = {p.name for p in d.iterdir()} if d.is_dir() else set()
    for miss in sorted(want - have):
        errs.append(f"{d.name}/{miss} is missing")
    for extra in sorted(have - want):
        errs.append(f"{d.name}/{extra} is not expected")
    for n in G.NAMES:
        path = d / f"{n}.png"
        if not path.exists():
            continue
        try:
            w, h, depth, ctype, inter, pal, trns, rows = read_png(path.read_bytes())
        except Exception as e:  # noqa: BLE001
            errs.append(f"{path.name}: {e}")
            continue
        if (w, h) != (12, 12):
            errs.append(f"{path.name}: {w}x{h}, not 12x12")
        if (depth, ctype, inter) != (8, 3, 0):
            errs.append(f"{path.name}: needs 8-bit palette, not interlaced "
                        f"(depth {depth}, colour type {ctype}, interlace {inter})")
            continue
        if pal[:5] != G.ROLE_RGB:
            errs.append(f"{path.name}: palette {pal[:5]} is not the role colours")
        if not trns or trns[0] != 0:
            errs.append(f"{path.name}: palette index 0 is not transparent")
        used = {v for r in rows for v in r}
        if not used <= {0, 1, 2, 3, 4}:
            errs.append(f"{path.name}: indices {sorted(used)} outside 0..4")
        if not {1, 2, 3} <= used:
            errs.append(f"{path.name}: needs a rim, a body and a letter (roles {sorted(used)})")
        touch = [(x, y) for y in range(12) for x in range(12) if rows[y][x] == G.LETTER
                 and any(0 <= y + dy < 12 and 0 <= x + dx < 12 and rows[y + dy][x + dx] == G.OUTLINE
                         for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)))]
        if touch:
            errs.append(f"{path.name}: the letter touches the rim at {touch[0][0]},{touch[0][1]}")
        if rows != G.glyph(n):
            errs.append(f"{path.name}: differs from what tools/generate_switch_glyphs.py draws")
    for p, data in G.build().items():
        if p.suffix == ".inc" and (not p.exists() or p.read_bytes() != data):
            errs.append(f"{p.relative_to(ROOT)} is out of date (run tools/generate_switch_glyphs.py)")
    lic = d / "LICENSE"
    if lic.exists() and "CC0" not in lic.read_text():
        errs.append("LICENSE does not say CC0")
    prov = d / "PROVENANCE.md"
    if prov.exists():
        text = " ".join(prov.read_text().lower().split())
        for w in PROVENANCE_WORDS:
            if w not in text:
                errs.append(f"PROVENANCE.md does not state: {w!r}")
    return errs


def main() -> int:
    errs = check()
    if errs:
        print("assets/glyphs check failed:", *errs, sep="\n  ")
        return 1
    print(f"ok: {len(G.NAMES)} glyphs, 12x12 palette PNGs, art, licence and provenance")
    return 0


if __name__ == "__main__":
    sys.exit(main())
