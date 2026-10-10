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

On the original disc the boot TIM lives in the executable's tail, loaded at
OVERLAY_VRAM (see vendor/dw3_decomp docs and main()'s BOOT_IMAGE_RECT).

Phase 1 pack_runtime.py does not invent those bytes. A decomp/assets extract
of the tail will produce boot_tim.bin here in Phase 2. Until then, hosts that
need a boot framebuffer should stub or skip LoadImage of the boot TIM.
"""


def _count_files(root: Path) -> int:
    if not root.is_dir():
        return 0
    return sum(1 for p in root.rglob("*") if p.is_file())


def pack(version: str, disks: Path, out: Path) -> int:
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

    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    aaa_dst = out / "AAA"
    shutil.copytree(aaa_src, aaa_dst)
    shutil.copy2(exe_src, out / exe_name)
    shutil.copy2(map_src, out / map_name)

    boot_src = disks / "boot_tim.bin"
    boot_note = "BOOT_IMAGE.txt (stub)"
    if boot_src.is_file():
        shutil.copy2(boot_src, out / "boot_tim.bin")
        boot_note = "boot_tim.bin (copied from disks)"
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
    args = parser.parse_args(argv)
    return pack(args.version, args.disks, args.out)


if __name__ == "__main__":
    sys.exit(main())
