#!/usr/bin/env python3
"""Dump the font icon cells (the PS button icons among them) from the owner's runtime.

P10.6 step 0 (docs/P10.6_GLYPHS.md).  Reads CMFONTTM.BIN (FILE_FONT) from
runtime/<us|eu>/, rebuilds the two font pages the way loadFont() does, reads the
Glyph tables FONT_ICONS_1..3 / FONT_ICON_MAP from the decomp source for the
chosen version, and writes:

  <out>/<ver>/page0.png page1.png   the two 256x256 4bpp pages with palette 0
  <out>/<ver>/icons_size<N>.png     contact sheet of the 114 icons, 4x scale
  <out>/<ver>/manifest.csv          size, n, sjis, page, u, v, w, h, dx, dy, advance
  stdout (--ascii CODE ...)         ASCII art of chosen icons (sjis hex codes)

Layout facts it relies on (decomp: src/engine/gfx/tim_loader.c, font.c,
text_window.c):
- the archive's TIMs are raw 16-bit blocks of 64x256 halfwords, loaded side by
  side 0x40 halfwords apart from VRAM x=0x140; page p is entry p. The game reads
  them as 4 bpp (tpage), so a page is 256x256 texels, low nibble first.
- every glyph uses clut x=48 and clutY 232 (US) or 224 (EU), plus the text
  window's palette number as a row offset: a 16-colour row of page 0, halfwords
  48..63, colour 0x0000 transparent. Many rows exist (one per text colour); the
  dump uses the base row.
- text icon n (1..114) is FONT_ICONS_k[n-1]; FONT_ICON_MAP gives sjis -> n.

Never commit the output: it is the owner's game art.  The default --out is under
tmp/ (git-ignored); the tool refuses a path git does not ignore.
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import extract_textures as X  # noqa: E402

FONT_PATH = {"us": "AAA/DAT/COMMON/CMFONTTM.BIN", "eu": "AAA/DAT/COUNTRY/CMFONTTM.BIN"}
PAL_X, PAL_Y = 48, 232  # PAL_Y is the US base row; the EU tables say 224
# Shift-JIS symbols a PS pad prompt would use (identity guess; the dump proves it)
CANDIDATES = {0x817E: "x", 0x819B: "circle?", 0x81A0: "square?", 0x81A2: "triangle?"}


def parse_tables(src: Path, version: str):
    """({size: [10-tuple per icon]}, {sjis: n}) for one version."""
    text = src.read_text()
    out = {}
    for size in (1, 2, 3):
        m = re.search(rf"Glyph FONT_ICONS_{size}\[114\] = \{{(.*?)\n\}};", text, re.S)
        body = m.group(1)
        parts = re.split(r"^#(?:if|elif)\s+(VERSION_\w+)\s*$|^#endif\s*$", body, flags=re.M)
        chosen = None
        i = 1
        while i < len(parts):
            if parts[i] == "VERSION_" + version.upper():
                chosen = parts[i + 1]
                break
            i += 2
        if chosen is None:
            raise SystemExit(f"no {version} branch for FONT_ICONS_{size}")
        cells = [tuple(int(x) for x in g.split(",")) for g in re.findall(r"\{([^{}]+)\}", chosen)]
        if len(cells) != 114:
            raise SystemExit(f"FONT_ICONS_{size} {version}: {len(cells)} cells, expected 114")
        out[size] = [c[:10] for c in cells]
    m = re.search(r"GlyphMap FONT_ICON_MAP\[\] = \{(.*?)\n\};", text, re.S)
    imap = {int(a, 16): int(b) for a, b in re.findall(r"\{\s*(0x[0-9A-Fa-f]+),\s*(\d+)\s*\}", m.group(1))}
    return out, imap


def load_pages(runtime: Path, version: str):
    buf = (runtime / version / FONT_PATH[version]).read_bytes()
    tims = [f.tim for f in X.walk(buf, [], [], 0)]
    if len(tims) < 2 or any(t.mode != 2 or t.image_w != 64 or t.height != 256 for t in tims[:2]):
        raise SystemExit("unexpected CMFONTTM layout")
    pages = []
    for t in tims[:2]:
        rows = []
        for y in range(256):
            r = []
            for b in t.pixels[y * 128:(y + 1) * 128]:
                r.append(b & 15)
                r.append(b >> 4)
            rows.append(r)
        pages.append(rows)
    p0 = tims[0].pixels

    def pal(row=PAL_Y):
        base = row * 128 + PAL_X * 2
        return [int.from_bytes(p0[base + i * 2:base + i * 2 + 2], "little") for i in range(16)]

    return pages, pal


def cell(pages, g):
    page, u, v, _, _, w, h = g[:7]
    return [pages[page][v + y][u:u + w] for y in range(h)]


def ascii_cell(c):
    return "\n".join("".join(" .:-=+*#%@abcdef"[p] if p else " " for p in r) for r in c)


def rgba(c):
    return X.ps1_rgba(c) if c else b"\x00\x00\x00\x00"


def rgba_table(palette, row):
    return [rgba(c) for c in palette(row)]


def ignored(path: Path) -> bool:
    return subprocess.run(["git", "check-ignore", "-q", str(path)], cwd=HERE.parent).returncode == 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runtime", default=os.path.expanduser("~/.cache/dw3-switch/runtime"))
    ap.add_argument("--decomp", default=str(HERE.parent / "vendor/dw3_decomp"))
    ap.add_argument("--out", default=str(HERE.parent / "tmp/evidence/font_cells"))
    ap.add_argument("--version", choices=("us", "eu", "both"), default="both")
    ap.add_argument("--ascii", nargs="*", default=[],
                    help="sjis hex codes to print, e.g. 81A2; 'buttons' = the 4 candidates")
    ap.add_argument("--pal-row", type=int, default=0,
                    help="CLUT row for PNG colours (default: each glyph's own clutY)")
    ap.add_argument("--no-files", action="store_true", help="only print ASCII, write nothing")
    a = ap.parse_args(argv)
    out = Path(a.out)
    if not a.no_files and not ignored(out / "x"):
        print(f"refusing to write game art to {out}: not git-ignored", file=sys.stderr)
        return 2
    codes = []
    for s in a.ascii:
        codes += list(CANDIDATES) if s == "buttons" else [int(s, 16)]
    src = Path(a.decomp) / "src/engine/text/text_tools.c"
    for ver in (("us", "eu") if a.version == "both" else (a.version,)):
        tables, imap = parse_tables(src, ver)
        pages, palette = load_pages(Path(a.runtime), ver)
        pal = palette(tables[1][0][4])
        print(f"== {ver}: base palette row {tables[1][0][4]} = {' '.join('%04X' % c for c in pal)}")
        rev = {n: c for c, n in imap.items()}
        for code in codes:
            n = imap.get(code)
            if n is None:
                print(f"{code:04X}: not in FONT_ICON_MAP")
                continue
            for size in (1, 2, 3):
                g = tables[size][n - 1]
                print(f"-- {ver} {code:04X} icon n={n} size{size} cell={g}")
                print(ascii_cell(cell(pages, g)))
        if a.no_files:
            continue
        d = out / ver
        d.mkdir(parents=True, exist_ok=True)
        for p in (0, 1):
            pt = rgba_table(palette, a.pal_row or tables[1][0][4])
            X.write_png(d / f"page{p}.png", 256, [b"".join(pt[i] for i in r) for r in pages[p]])
        with open(d / "manifest.csv", "w", newline="") as f:
            wr = csv.writer(f)
            wr.writerow(["size", "n", "sjis", "page", "u", "v", "w", "h", "dx", "dy", "advance"])
            for size in (1, 2, 3):
                for n, g in enumerate(tables[size], 1):
                    wr.writerow([size, n, "%04X" % rev.get(n, 0), g[0], g[1], g[2], g[5], g[6], g[7], g[8], g[9]])
        bg = b"\x28\x28\x28\xff"
        for size in (1, 2, 3):
            cols, sc, cw, ch = 19, 4, 16, 16
            W, H = cols * cw * sc, 6 * ch * sc
            sheet = [[bg] * W for _ in range(H)]
            for n, g in enumerate(tables[size], 1):
                if g[0] == 0xFF:
                    continue
                ox, oy = ((n - 1) % cols) * cw * sc, ((n - 1) // cols) * ch * sc
                gp = rgba_table(palette, a.pal_row or g[4])
                for y, r in enumerate(cell(pages, g)):
                    for x, p in enumerate(r):
                        px = gp[p] if p else bg
                        for j in range(sc):
                            for i in range(sc):
                                sheet[oy + y * sc + j][ox + x * sc + i] = px
            X.write_png(d / f"icons_size{size}.png", W, [b"".join(r) for r in sheet])
        print(f"wrote {d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
