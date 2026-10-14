#!/usr/bin/env python3
"""Export the owner's runtime textures (PS1 TIMs) to PNG for upscaling work.

Walks a runtime tree (runtime/<ver>/ from tools/pack_runtime.py), unpacks
the containers the engine itself unpacks, and writes every TIM it finds as
a PNG plus one manifest row, so an upscaled copy can later be matched back
to the exact TIM it came from (P8 texture packs, docs/TEXTURE_PACKS.md).

Containers (vendor/dw3_decomp):
- RLEN: magic "RLEN", unpacked size, then runs; a byte with bit 7 repeats
  the next byte (b & 0x7F) times, otherwise copies b literal bytes, 0 ends
  (src/engine/file/decompressor.c).
- Archive: a table of u32 offsets from the archive start, ended by 0
  (getArchiveEntry in src/engine/file/file_cache.c). Entries nest: map
  tiles are archives of RLEN tiles whose entry 0 is a TIM.
- Anything else is scanned for TIMs on 4-byte boundaries (card images,
  overlays, the executable).

Output under --out:
  <relative file path>/<chain>.png   chain = e<i> per archive entry,
                                     @<hex offset> for scanned TIMs,
                                     "root" for a TIM at the file start
  manifest.csv                       one row per PNG
  summary.json                       counts per source and per format

PNG colours: palette 0 for 4/8-bit TIMs (--all-palettes writes .pN.png for
the others). The PS1 colour 0x0000 is transparent (alpha 0); other colours
are opaque, the STP bit is only counted in the manifest. Keys for a later
replacement are the SHA-1 of the TIM pixel block and of its CLUT block.

Never commit the output: it is the owner's game art.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterator, List, Optional, Tuple

RLEN_MAGIC = b"RLEN"
TIM_MAGIC = b"\x10\x00\x00\x00"
MAX_ARCHIVE_ENTRIES = 4096
MAX_DEPTH = 6
SKIP_SUFFIXES = {".STR"}  # movies: MDEC frames, no TIMs
SKIP_DIRS = {"memcard"}

BPP_NAMES = {0: "4bpp", 1: "8bpp", 2: "16bpp", 3: "24bpp"}


@dataclass
class Tim:
    offset: int
    size: int
    mode: int
    width: int  # pixels
    height: int
    image_x: int
    image_y: int
    image_w: int  # halfwords
    pixels: bytes
    clut_x: int = -1
    clut_y: int = -1
    clut_w: int = 0
    clut_h: int = 0
    clut: bytes = b""


def parse_tim(buf: bytes, off: int) -> Optional[Tim]:
    """A TIM at buf[off:], or None when the header or sizes do not fit."""
    end = len(buf)
    if off + 8 > end or buf[off : off + 4] != TIM_MAGIC:
        return None
    flag = struct.unpack_from("<I", buf, off + 4)[0]
    if flag & ~0xF:
        return None
    mode = flag & 7
    if mode > 3:
        return None
    p = off + 8
    tim = Tim(off, 0, mode, 0, 0, 0, 0, 0, b"")
    if flag & 8:
        if p + 12 > end:
            return None
        size, x, y, w, h = struct.unpack_from("<IHHHH", buf, p)
        if w == 0 or h == 0 or x >= 1024 or y >= 512 or w > 1024 or h > 512:
            return None
        if size != 12 + w * h * 2 or p + size > end:
            return None
        if mode > 1:
            return None  # a CLUT on a direct-colour TIM: not a TIM
        tim.clut_x, tim.clut_y, tim.clut_w, tim.clut_h = x, y, w, h
        tim.clut = bytes(buf[p + 12 : p + size])
        p += size
    if p + 12 > end:
        return None
    size, x, y, w, h = struct.unpack_from("<IHHHH", buf, p)
    if w == 0 or h == 0 or x >= 1024 or y >= 512 or w > 1024 or h > 512:
        return None
    if size != 12 + w * h * 2 or p + size > end:
        return None
    tim.image_x, tim.image_y, tim.image_w, tim.height = x, y, w, h
    tim.width = {0: w * 4, 1: w * 2, 2: w, 3: (w * 2) // 3}[mode]
    if tim.width == 0:
        return None
    tim.pixels = bytes(buf[p + 12 : p + size])
    tim.size = p + size - off
    return tim


def rlen_unpack(buf: bytes) -> Optional[bytes]:
    """Unpack an RLEN block, or None when it is not one or overruns."""
    if len(buf) < 9 or buf[:4] != RLEN_MAGIC:
        return None
    size = struct.unpack_from("<I", buf, 4)[0]
    if size == 0 or size > 64 << 20:
        return None
    out = bytearray()
    i = 8
    n = len(buf)
    while i < n:
        c = buf[i]
        if c == 0:
            return bytes(out)
        if c & 0x80:
            if i + 1 >= n:
                return None
            out += bytes((buf[i + 1],)) * (c & 0x7F)
            i += 2
        else:
            if i + 1 + c > n:
                return None
            out += buf[i + 1 : i + 1 + c]
            i += 1 + c
        if len(out) > size + 0x10000:
            return None
    return None


def archive_entries(buf: bytes) -> Optional[List[Tuple[int, int]]]:
    """(start, end) of each entry when buf looks like an offset-table archive."""
    n = len(buf)
    if n < 8:
        return None
    offsets: List[int] = []
    for i in range(min(MAX_ARCHIVE_ENTRIES, n // 4)):
        v = struct.unpack_from("<I", buf, i * 4)[0]
        if v == 0:
            break
        offsets.append(v)
    else:
        return None
    if not offsets:
        return None
    table_end = (len(offsets) + 1) * 4
    first = offsets[0]
    # entries follow the table (allowing sector padding), in order, aligned
    if first < table_end or first > table_end + 0x800:
        return None
    prev = 0
    for v in offsets:
        if v & 3 or v < prev or v > n:
            return None
        prev = v
    if len(offsets) == 1 and first == n:
        return None
    bounds = offsets + [n]
    return [(bounds[i], bounds[i + 1]) for i in range(len(offsets))]


def scan_tims(buf: bytes) -> Iterator[Tim]:
    """Every TIM on a 4-byte boundary, skipping over the ones found."""
    pos = 0
    while True:
        pos = buf.find(TIM_MAGIC, pos)
        if pos < 0:
            return
        if pos & 3:
            pos += 4 - (pos & 3)
            continue
        tim = parse_tim(buf, pos)
        if tim is None:
            pos += 4
            continue
        yield tim
        pos += (tim.size + 3) & ~3


@dataclass
class Found:
    chain: str
    tim: Tim
    via: str  # how the containers were unpacked, e.g. "archive>rlen>archive"


def walk(buf: bytes, chain: List[str], via: List[str], depth: int) -> Iterator[Found]:
    if depth > MAX_DEPTH:
        return
    unpacked = rlen_unpack(buf) if buf[:4] == RLEN_MAGIC else None
    if unpacked is not None:
        yield from walk(unpacked, chain, via + ["rlen"], depth + 1)
        return
    tim = parse_tim(buf, 0)
    if tim is not None and tim.size >= len(buf) - 0x800:
        yield Found("_".join(chain) or "root", tim, ">".join(via))
        return
    entries = archive_entries(buf)
    if entries is not None:
        for i, (a, b) in enumerate(entries):
            if b > a:
                yield from walk(buf[a:b], chain + [f"e{i:03d}"], via + ["archive"], depth + 1)
        return
    for tim in scan_tims(buf):
        name = chain + [f"@{tim.offset:06x}"]
        yield Found("_".join(name), tim, ">".join(via + ["scan"]))


def ps1_rgba(c: int) -> bytes:
    if c == 0:
        return b"\x00\x00\x00\x00"
    r = c & 31
    g = (c >> 5) & 31
    b = (c >> 10) & 31
    return bytes(((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 255))


_RGBA_CACHE = [ps1_rgba(c) for c in range(0x10000)]


def palettes(tim: Tim) -> List[List[bytes]]:
    """CLUT rows split into palettes of 16 (4-bit) or 256 (8-bit) colours."""
    colours = struct.unpack(f"<{len(tim.clut) // 2}H", tim.clut) if tim.clut else ()
    per = 16 if tim.mode == 0 else 256
    if not colours:
        # no CLUT in the file: grey ramp so the indices stay visible
        step = 255 // (per - 1)
        return [[bytes((i * step, i * step, i * step, 255)) for i in range(per)]]
    pals = []
    for start in range(0, len(colours), per):
        pal = [_RGBA_CACHE[c] for c in colours[start : start + per]]
        pal += [b"\x00\x00\x00\x00"] * (per - len(pal))
        pals.append(pal)
    return pals


def tim_rows(tim: Tim, pal: Optional[List[bytes]]) -> List[bytes]:
    stride = tim.image_w * 2
    rows = []
    for y in range(tim.height):
        line = tim.pixels[y * stride : (y + 1) * stride]
        if tim.mode == 0:
            idx = []
            for b in line:
                idx.append(b & 15)
                idx.append(b >> 4)
            rows.append(b"".join(pal[i] for i in idx[: tim.width]))
        elif tim.mode == 1:
            rows.append(b"".join(pal[b] for b in line[: tim.width]))
        elif tim.mode == 2:
            vals = struct.unpack(f"<{tim.width}H", line[: tim.width * 2])
            rows.append(b"".join(_RGBA_CACHE[v] for v in vals))
        else:
            out = bytearray()
            for x in range(tim.width):
                out += line[x * 3 : x * 3 + 3] + b"\xff"
            rows.append(bytes(out))
    return rows


def write_png(path: Path, width: int, rows: List[bytes]) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(kind + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + r for r in rows)
    ihdr = struct.pack(">IIBBBBB", width, len(rows), 8, 6, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


MANIFEST_FIELDS = [
    "png",
    "file_id",
    "source",
    "chain",
    "via",
    "offset",
    "format",
    "width",
    "height",
    "vram_x",
    "vram_y",
    "clut_x",
    "clut_y",
    "clut_w",
    "clut_h",
    "palettes",
    "stp_texels",
    "pixels_sha1",
    "clut_sha1",
]


def stp_count(tim: Tim) -> int:
    if tim.mode == 2:
        vals = struct.unpack(f"<{len(tim.pixels) // 2}H", tim.pixels)
        return sum(1 for v in vals if v & 0x8000)
    if tim.clut:
        vals = struct.unpack(f"<{len(tim.clut) // 2}H", tim.clut)
        return sum(1 for v in vals if v & 0x8000)
    return 0


def load_file_ids(root: Path) -> Dict[str, int]:
    ids: Dict[str, int] = {}
    for m in sorted(root.glob("files_*.json")):
        data = json.loads(m.read_text(encoding="utf-8"))
        for k, v in data.get("files", {}).items():
            ids[str(v.get("path", "")).replace("\\", "/").lstrip("/").upper()] = int(k)
    return ids


def source_files(root: Path) -> List[Path]:
    out = []
    for p in sorted(root.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(root)
        if rel.parts and rel.parts[0] in SKIP_DIRS:
            continue
        if p.suffix.upper() in SKIP_SUFFIXES or p.suffix == ".json":
            continue
        if p.name.startswith(".") or p.name.endswith(".dw3-replace-backup"):
            continue
        out.append(p)
    return out


def extract(root: Path, out: Path, all_palettes: bool = False, quiet: bool = False) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    ids = load_file_ids(root)
    files = source_files(root)
    stats = {"files_scanned": 0, "files_with_textures": 0, "pngs": 0, "tims": 0,
             "unique_tims": 0, "by_format": {}, "by_top_dir": {}}
    seen = set()
    with (out / "manifest.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=MANIFEST_FIELDS)
        writer.writeheader()
        for n, path in enumerate(files, 1):
            rel = path.relative_to(root).as_posix()
            stats["files_scanned"] += 1
            data = path.read_bytes()
            found = list(walk(data, [], [], 0))
            if not found:
                continue
            stats["files_with_textures"] += 1
            top = rel.split("/")[1] if rel.startswith("AAA/") and rel.count("/") >= 2 else rel.split("/")[0]
            dest_dir = out / rel
            dest_dir.mkdir(parents=True, exist_ok=True)
            for f in found:
                tim = f.tim
                fmt = BPP_NAMES[tim.mode]
                pals = palettes(tim) if tim.mode < 2 else [None]
                key = hashlib.sha1(tim.pixels).hexdigest()
                ckey = hashlib.sha1(tim.clut).hexdigest() if tim.clut else ""
                stats["tims"] += 1
                if (key, ckey) not in seen:
                    seen.add((key, ckey))
                    stats["unique_tims"] += 1
                stats["by_format"][fmt] = stats["by_format"].get(fmt, 0) + 1
                stats["by_top_dir"][top] = stats["by_top_dir"].get(top, 0) + 1
                count = len(pals) if all_palettes else 1
                for pi in range(count):
                    name = f"{f.chain}.png" if pi == 0 else f"{f.chain}.p{pi}.png"
                    write_png(dest_dir / name, tim.width, tim_rows(tim, pals[pi]))
                    stats["pngs"] += 1
                    writer.writerow({
                        "png": f"{rel}/{name}",
                        "file_id": ids.get(rel.upper(), ""),
                        "source": rel,
                        "chain": f.chain,
                        "via": f.via,
                        "offset": f"0x{tim.offset:x}",
                        "format": fmt + ("" if tim.clut or tim.mode > 1 else "-noclut"),
                        "width": tim.width,
                        "height": tim.height,
                        "vram_x": tim.image_x,
                        "vram_y": tim.image_y,
                        "clut_x": tim.clut_x,
                        "clut_y": tim.clut_y,
                        "clut_w": tim.clut_w,
                        "clut_h": tim.clut_h,
                        "palettes": len(pals) if tim.mode < 2 else 0,
                        "stp_texels": stp_count(tim),
                        "pixels_sha1": key,
                        "clut_sha1": ckey,
                    })
            if not quiet and n % 100 == 0:
                print(f"[{n}/{len(files)}] {stats['tims']} TIMs", file=sys.stderr)
    (out / "summary.json").write_text(json.dumps(stats, indent=2) + "\n", encoding="utf-8")
    return stats


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--runtime", required=True, type=Path, help="runtime/<ver> tree")
    ap.add_argument("--out", required=True, type=Path, help="output directory (outside Git)")
    ap.add_argument("--all-palettes", action="store_true",
                    help="also write each extra CLUT palette as <chain>.p<N>.png")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    if not args.runtime.is_dir():
        print(f"error: {args.runtime} is not a directory", file=sys.stderr)
        return 2
    stats = extract(args.runtime, args.out, args.all_palettes, args.quiet)
    print(json.dumps(stats, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
