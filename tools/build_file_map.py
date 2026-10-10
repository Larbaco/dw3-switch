#!/usr/bin/env python3
"""Map Digimon World 3 disc LBAs to ISO9660 paths via FILE_SECTORS.

Reads a MODE2/2352 .bin (same layout as vendor/dw3_decomp/tools/extract_disc.py)
and a file_table.c snippet with VERSION_US / VERSION_EU arrays. Does not invoke
the C preprocessor — version blocks are selected with a small state machine.

.STR paths get form 2 (Phase 3 movies may need raw-sector packing later; Phase 1
pack may still copy 2048-byte user data).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

SECTOR = 2352
USER_DATA = 24
LOGICAL = 2048

EXECUTABLE = {
    "us": "SLUS_014.36",
    "eu": "SLES_039.36",
}

VERSION_MACRO = {
    "us": "VERSION_US",
    "eu": "VERSION_EU",
}


def read_sector(image: bytes, lba: int) -> bytes:
    offset = lba * SECTOR + USER_DATA
    return image[offset : offset + LOGICAL]


def walk_iso(image: bytes) -> Dict[int, Dict[str, int | str]]:
    """Return {lba: {path, bytes}} for every non-directory file."""
    pvd = read_sector(image, 16)
    pt_size = struct.unpack_from("<I", pvd, 132)[0]
    pt_lba = struct.unpack_from("<I", pvd, 140)[0]
    pt = b"".join(
        read_sector(image, pt_lba + i) for i in range((pt_size + LOGICAL - 1) // LOGICAL)
    )[:pt_size]

    dirs: List[Tuple[str, int, int]] = []
    i = 0
    while i < len(pt):
        name_len = pt[i]
        extent = struct.unpack_from("<I", pt, i + 2)[0]
        parent = struct.unpack_from("<H", pt, i + 6)[0]
        name = pt[i + 8 : i + 8 + name_len].decode("ascii")
        dirs.append((name, extent, parent))
        i += 8 + name_len + (name_len & 1)

    def dir_path(k: int) -> str:
        name, _, parent = dirs[k]
        if k == 0:
            return ""
        return os.path.join(dir_path(parent - 1), name)

    files: Dict[int, Dict[str, int | str]] = {}
    for k, (_, extent, _) in enumerate(dirs):
        first = read_sector(image, extent)
        size = struct.unpack_from("<I", first, 10)[0]
        data = b"".join(
            read_sector(image, extent + j)
            for j in range((size + LOGICAL - 1) // LOGICAL)
        )
        j = 0
        while j < len(data):
            rec_len = data[j]
            if rec_len == 0:
                j = (j // LOGICAL + 1) * LOGICAL
                continue
            rec = data[j : j + rec_len]
            j += rec_len
            flags = rec[25]
            name = rec[33 : 33 + rec[32]]
            if flags & 2 or name in (b"\x00", b"\x01"):
                continue
            lba = struct.unpack_from("<I", rec, 2)[0]
            length = struct.unpack_from("<I", rec, 10)[0]
            path = os.path.join(dir_path(k), name.decode("ascii").split(";")[0])
            path = path.replace("\\", "/")
            files[lba] = {"path": path, "bytes": length}
    return files


_ARRAY_START = re.compile(
    r"^\s*(?:s32|u16|int|short|unsigned\s+short)\s+(FILE_SECTORS|FILE_SECTOR_COUNTS)\s*\["
)
_INT = re.compile(r"0x[0-9A-Fa-f]+|\d+")


def parse_file_table(text: str, version: str) -> Tuple[List[int], List[int]]:
    """Extract FILE_SECTORS and FILE_SECTOR_COUNTS for VERSION_US/EU via #if SM."""
    want = VERSION_MACRO[version]
    active = False
    depth = 0  # nesting of #if/#ifdef while active (or waiting)
    in_target = False
    current_array: Optional[str] = None
    sectors: List[int] = []
    counts: List[int] = []
    collecting: Optional[List[int]] = None

    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("#"):
            # Strip block comments on the same line for simplicity.
            directive = line.split("//", 1)[0].strip()
            tokens = directive.split()
            if not tokens:
                continue
            cmd = tokens[0]
            if cmd in ("#if", "#ifdef", "#ifndef"):
                if in_target:
                    depth += 1
                elif cmd == "#if" and len(tokens) >= 2 and tokens[1] == want:
                    in_target = True
                    depth = 1
                    active = True
                elif not in_target:
                    # Enter a skipped sibling/other block so #elif/#endif track.
                    pass
            elif cmd == "#elif":
                if in_target and depth == 1:
                    # Leaving our block for a sibling.
                    in_target = False
                    active = False
                    depth = 0
                    current_array = None
                    collecting = None
                elif not in_target and depth == 0:
                    if len(tokens) >= 2 and tokens[1] == want:
                        in_target = True
                        depth = 1
                        active = True
            elif cmd == "#else":
                if in_target and depth == 1:
                    in_target = False
                    active = False
                    depth = 0
                    current_array = None
                    collecting = None
            elif cmd == "#endif":
                if in_target:
                    depth -= 1
                    if depth == 0:
                        in_target = False
                        active = False
                        current_array = None
                        collecting = None
            continue

        if not active:
            continue

        m = _ARRAY_START.match(line)
        if m:
            current_array = m.group(1)
            # Only collect numeric initializers after '{', never the [N] size
            # or type tokens like s32 / u16.
            if "{" in line:
                collecting = sectors if current_array == "FILE_SECTORS" else counts
                after = line.split("{", 1)[1]
                for tok in _INT.findall(after):
                    collecting.append(int(tok, 0))
                if "};" in line:
                    collecting = None
                    current_array = None
            else:
                collecting = None  # wait for '{' on a later line
            continue

        if current_array and collecting is None and "{" in line:
            collecting = sectors if current_array == "FILE_SECTORS" else counts
            after = line.split("{", 1)[1]
            for tok in _INT.findall(after):
                collecting.append(int(tok, 0))
            if "};" in line:
                collecting = None
                current_array = None
            continue

        if collecting is None:
            continue

        for tok in _INT.findall(line):
            collecting.append(int(tok, 0))

        if "};" in line:
            collecting = None
            current_array = None

    if not sectors:
        raise SystemExit(f"FILE_SECTORS not found for {want}")
    if not counts:
        raise SystemExit(f"FILE_SECTOR_COUNTS not found for {want}")
    return sectors, counts


def build_map(
    image: bytes,
    version: str,
    sectors: List[int],
    counts: List[int],
) -> dict:
    by_lba = walk_iso(image)
    files = {}
    n = min(len(sectors), len(counts))
    for file_id in range(n):
        lba = sectors[file_id]
        if lba == 0:
            continue
        meta = by_lba.get(lba)
        if meta is None:
            # Leave absent; --check will catch missing paths.
            continue
        path = str(meta["path"])
        form = 2 if path.upper().endswith(".STR") else 1
        sector_count = counts[file_id]
        files[str(file_id)] = {
            "path": path,
            "lba": lba,
            "sectors": sector_count,
            "bytes": int(meta["bytes"]),
            "form": form,
        }
    return {
        "version": version,
        "executable": EXECUTABLE[version],
        "files": files,
    }


def check_map(
    data: dict,
    sectors: List[int],
    allow_missing: Iterable[int],
) -> int:
    allow = set(allow_missing)
    mapped = {int(k) for k in data.get("files", {})}
    missing = []
    for file_id, lba in enumerate(sectors):
        if lba == 0:
            continue
        if file_id in allow:
            continue
        if file_id not in mapped:
            missing.append(file_id)
    if missing:
        print(
            "missing paths for non-zero FILE_SECTORS ids: "
            + ", ".join(str(i) for i in missing),
            file=sys.stderr,
        )
        return 1
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", type=Path, help="MODE2/2352 disc image")
    parser.add_argument("--version", choices=("us", "eu"), help="game version")
    parser.add_argument("--file-table", type=Path, required=True)
    parser.add_argument("--out", type=Path, help="write files_*.json")
    parser.add_argument("--check", type=Path, help="validate an existing map JSON")
    parser.add_argument(
        "--allow-missing",
        type=int,
        action="append",
        default=[],
        metavar="ID",
        help="file id allowed to lack a path during --check (repeatable)",
    )
    args = parser.parse_args(argv)

    text = args.file_table.read_text(encoding="utf-8", errors="replace")

    if args.check is not None:
        data = json.loads(args.check.read_text(encoding="utf-8"))
        version = data.get("version") or args.version
        if version not in VERSION_MACRO:
            print("map JSON missing/invalid version", file=sys.stderr)
            return 1
        sectors, _counts = parse_file_table(text, version)
        return check_map(data, sectors, args.allow_missing)

    if args.bin is None or args.version is None or args.out is None:
        parser.error("--bin, --version, and --out are required unless --check is set")

    sectors, counts = parse_file_table(text, args.version)
    image = args.bin.read_bytes()
    data = build_map(image, args.version, sectors, counts)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(data, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
