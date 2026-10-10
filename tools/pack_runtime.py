#!/usr/bin/env python3
"""Pack disks/<ver>/ extract into the shared PC/Switch runtime tree.

Copies AAA/, the version executable, and files_<ver>.json into --out.
The boot TIM (executable tail at OVERLAY_VRAM) is Phase 2; until then a
BOOT_IMAGE.txt stub is written instead of inventing boot_tim.bin bytes.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path
from typing import List, Optional

EXECUTABLE = {
    "us": "SLUS_014.36",
    "eu": "SLES_039.36",
}

BOOT_IMAGE_STUB = """\
Boot image (boot_tim.bin) — not packed yet
==========================================

On the original disc the boot TIM lives in the executable's tail at
STAGE_VRAM / SUB_OVERLAY_ADDRESS (see vendor/dw3_decomp docs and main()'s
BOOT_IMAGE_RECT). pack_runtime.py extracts it from the version executable
when possible; if that fails, hosts should stub or skip LoadImage.
"""

# US / EU load addresses from vendor/dw3_decomp docs/binaries.md
_EXE_LOAD = 0x80010000
_STAGE_VRAM = {
    "us": 0x800A4CA4,
    "eu": 0x800A5DE0,
}


def _extract_boot_tim(exe: Path, version: str, out: Path) -> bool:
    """Pull the 320x480 16-bit TIM from the PS-X EXE at STAGE_VRAM."""
    import struct

    data = exe.read_bytes()
    if len(data) < 0x800 or data[:8] != b"PS-X EXE":
        return False
    stage = _STAGE_VRAM[version]
    off = 0x800 + (stage - _EXE_LOAD)
    if off + 8 > len(data) or data[off : off + 4] != b"\x10\x00\x00\x00":
        return False
    flag = struct.unpack_from("<I", data, off + 4)[0]
    pos = off + 8
    if flag & 8:
        if pos + 4 > len(data):
            return False
        clut_size = struct.unpack_from("<I", data, pos)[0]
        pos += clut_size
    if pos + 4 > len(data):
        return False
    img_size = struct.unpack_from("<I", data, pos)[0]
    end = pos + img_size
    if end > len(data) or img_size < 12:
        return False
    out.write_bytes(data[off:end])
    return True


def _count_files(root: Path) -> int:
    if not root.is_dir():
        return 0
    return sum(1 for p in root.rglob("*") if p.is_file())


def _prepare_out(out: Path, force: bool) -> int:
    """Ensure --out is ready. Refuse to wipe a non-empty tree without --force."""
    if not out.exists():
        out.mkdir(parents=True)
        return 0
    if not out.is_dir():
        print(f"error: --out is not a directory: {out}", file=sys.stderr)
        return 1
    if any(out.iterdir()):
        if not force:
            print(
                f"error: {out} exists and is non-empty; "
                f"pass --force to wipe it before packing",
                file=sys.stderr,
            )
            return 1
        shutil.rmtree(out)
        out.mkdir(parents=True)
        return 0
    return 0


def pack(version: str, disks: Path, out: Path, *, force: bool = False) -> int:
    map_name = f"files_{version}.json"
    map_src = disks / map_name
    if not map_src.is_file():
        print(
            f"error: {map_src} missing — run disc extract + build_file_map.py "
            f"(Task 7 / docs/DISC_SETUP.md) before packing",
            file=sys.stderr,
        )
        return 1

    exe_name = EXECUTABLE[version]
    exe_src = disks / exe_name
    if not exe_src.is_file():
        print(f"error: executable not found: {exe_src}", file=sys.stderr)
        return 1

    aaa_src = disks / "AAA"
    if not aaa_src.is_dir():
        print(f"error: AAA directory not found: {aaa_src}", file=sys.stderr)
        return 1

    if _prepare_out(out, force) != 0:
        return 1

    aaa_dst = out / "AAA"
    shutil.copytree(aaa_src, aaa_dst)
    shutil.copy2(exe_src, out / exe_name)
    shutil.copy2(map_src, out / map_name)

    boot_src = disks / "boot_tim.bin"
    boot_dst = out / "boot_tim.bin"
    boot_note = "BOOT_IMAGE.txt (stub)"
    if boot_src.is_file():
        shutil.copy2(boot_src, boot_dst)
        boot_note = "boot_tim.bin (copied from disks)"
    elif _extract_boot_tim(exe_src, version, boot_dst):
        boot_note = "boot_tim.bin (extracted from executable)"
    else:
        (out / "BOOT_IMAGE.txt").write_text(BOOT_IMAGE_STUB, encoding="utf-8")

    aaa_files = _count_files(aaa_dst)
    print(f"packed {version} -> {out}")
    print(f"  AAA files:     {aaa_files}")
    print(f"  executable:    {exe_name}")
    print(f"  file map:      {map_name}")
    print(f"  boot image:    {boot_note}")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", choices=("us", "eu"), required=True)
    parser.add_argument(
        "--disks",
        type=Path,
        required=True,
        help="extract root (e.g. disks/us)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        required=True,
        help="runtime output root (e.g. runtime/us)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="wipe a non-empty --out directory before packing",
    )
    args = parser.parse_args(argv)
    return pack(args.version, args.disks, args.out, force=args.force)


if __name__ == "__main__":
    sys.exit(main())
