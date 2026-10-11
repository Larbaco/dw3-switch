#!/usr/bin/env python3
"""Rewrite form-2 STR files as raw Mode2/2352 sectors.

The 2048-byte extract keeps the first 16 XA ADPCM blocks and drops the last
two. The decoder still emits 18 blocks, so the missing pair becomes silence
and the soundtrack runs long. `str_stream` plays a file whose size is
`sectors * 2352` as raw Mode2, which keeps all 18 blocks.

The file map is unchanged: LBA lookup still uses the map's sector count.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import List, Optional

RAW_SECTOR = 2352


def form2_entries(file_map: dict) -> List[dict]:
    files = file_map.get("files") or {}
    out = []
    for ent in files.values():
        if not isinstance(ent, dict):
            continue
        if ent.get("form") != 2:
            continue
        path = ent.get("path") or ""
        if not path.upper().endswith(".STR"):
            continue
        out.append(ent)
    return out


def restore_entry(runtime: Path, bin_fp, ent: dict, *, force: bool = False) -> str:
    """Copy one STR from the disc image. Returns 'wrote', 'skip', or 'missing'."""
    rel = ent["path"]
    sectors = int(ent["sectors"])
    lba = int(ent["lba"])
    dest = runtime / rel
    expect = sectors * RAW_SECTOR
    if dest.is_file() and dest.stat().st_size == expect and not force:
        return "skip"
    if not dest.is_file():
        return "missing"
    tmp = dest.with_suffix(dest.suffix + ".tmp")
    bin_fp.seek(lba * RAW_SECTOR)
    with tmp.open("wb") as out:
        remaining = expect
        while remaining:
            want = min(remaining, 1024 * 1024)
            chunk = bin_fp.read(want)
            if len(chunk) != want:
                tmp.unlink(missing_ok=True)
                raise IOError(f"short read from disc image at LBA {lba} ({rel})")
            out.write(chunk)
            remaining -= len(chunk)
    tmp.replace(dest)
    return "wrote"


def restore(runtime: Path, disc_bin: Path, *, force: bool = False) -> int:
    map_path = None
    for name in ("files_us.json", "files_eu.json"):
        candidate = runtime / name
        if candidate.is_file():
            map_path = candidate
            break
    if map_path is None:
        print(f"error: no files_us.json or files_eu.json under {runtime}", file=sys.stderr)
        return 1
    if not disc_bin.is_file():
        print(f"error: disc image not found: {disc_bin}", file=sys.stderr)
        return 1
    file_map = json.loads(map_path.read_text(encoding="utf-8"))
    entries = form2_entries(file_map)
    if not entries:
        print(f"error: no form-2 STR entries in {map_path}", file=sys.stderr)
        return 1
    wrote = skipped = missing = 0
    with disc_bin.open("rb") as bin_fp:
        for ent in entries:
            status = restore_entry(runtime, bin_fp, ent, force=force)
            rel = ent["path"]
            if status == "wrote":
                wrote += 1
                print(f"restored {rel} ({ent['sectors']} x {RAW_SECTOR})")
            elif status == "skip":
                skipped += 1
                print(f"already raw {rel}")
            else:
                missing += 1
                print(f"missing {rel}", file=sys.stderr)
    print(f"form2 STR: wrote={wrote} skip={skipped} missing={missing}")
    return 1 if missing else 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True, help="runtime/<ver> root")
    parser.add_argument("--bin", type=Path, required=True, help="Mode2/2352 disc image")
    parser.add_argument("--force", action="store_true", help="rewrite files that are already 2352")
    args = parser.parse_args(argv)
    return restore(args.runtime, args.bin, force=args.force)


if __name__ == "__main__":
    sys.exit(main())
