#!/usr/bin/env python3
"""Prefix defined globals in a stage .o and merge writable sections for restore.

  prefix_stage_obj.py wstag780 in.o out.o
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "tools" / "merge_ovl_sections.ld"


def _tool(name: str) -> str:
    """Resolve a binutils tool that can read the objects being packed.

    These are target objects, so a cross build must use the *target* binutils —
    a host `nm`/`objcopy`/`ld` cannot read or merge AArch64 objects. Callers pass
    the toolchain through the environment (the Switch build does); with nothing
    set we use the bare name, which is what the PC build has always done and
    must keep doing.
    """
    return os.environ.get(f"DW3_{name.upper()}") or (
        os.environ.get("DW3_BINUTILS_PREFIX", "") + name
    )


def nm_defined(path: Path) -> list[str]:
    out = subprocess.check_output(
        [_tool("nm"), "-g", "--defined-only", str(path)], text=True
    )
    names: list[str] = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        typ, name = parts[-2], parts[-1]
        if typ == "U":
            continue
        names.append(name)
    return names


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("stage", help="stage id, e.g. wstag780")
    ap.add_argument("src_o")
    ap.add_argument("dst_o")
    args = ap.parse_args()

    stage = args.stage.lower()
    prefix = stage.upper() + "_"
    src = Path(args.src_o)
    dst = Path(args.dst_o)
    dst.parent.mkdir(parents=True, exist_ok=True)

    redefs: list[str] = []
    for name in nm_defined(src):
        if name.startswith(prefix):
            continue
        if name.startswith(".L") or name.startswith("L0"):
            continue
        redefs.append(f"{name} {prefix}{name}")

    script_text = TEMPLATE.read_text().replace("__DW3_OVL__", stage)

    with tempfile.TemporaryDirectory(dir=str(dst.parent)) as tmp:
        tmp_path = Path(tmp)
        map_file = tmp_path / "redefs.txt"
        map_file.write_text("\n".join(redefs) + ("\n" if redefs else ""))
        prefixed = tmp_path / "prefixed.o"
        cmd = [_tool("objcopy")]
        if redefs:
            cmd += [f"--redefine-syms={map_file}"]
        cmd += [str(src), str(prefixed)]
        subprocess.check_call(cmd)

        script = tmp_path / "merge.ld"
        script.write_text(script_text)
        merged = tmp_path / "merged.o"
        subprocess.check_call(
            [_tool("ld"), "-r", "-T", str(script), "-o", str(merged), str(prefixed)]
        )
        shutil.copy2(merged, dst)

    print(
        f"prefix_stage_obj: {stage}: redefined {len(redefs)} symbols → {dst}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
