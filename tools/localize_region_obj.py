#!/usr/bin/env python3
"""P6.24: pack one region's game side into a single relocatable object.

`DW3_VERSION=both` compiles the game side twice (VERSION_US, VERSION_EU).
Each copy is turned into one object here so that two copies can sit in one
program:

  1. `ld -r` the copy's archives (pulling members from the root symbol, the way
     the final link would) into one relocatable object;
  2. make every global symbol local except the exports (default: only
     `dw3_game_api`, port/region/dw3_game_api.h), then give the export the
     region prefix (`us_dw3_game_api`);
  3. rename the writable overlay sections `dw3data_<x>` / `dw3bss_<x>` to
     `dw3data_<region>_<x>` and the matching undefined `__start_` / `__stop_`
     references of overlay_sections.c, so GNU ld does not merge the two
     regions' restore sections and each region's `__start_` / `__stop_` cover
     only its own.

Nothing here knows the game. The host symbols the copy needs (PSY-Z, the
platform, cdfs ...) stay undefined and global, so the final link resolves them
once, with the same link-order and --wrap rules as a plain build (PSY-Z
overrides keep winning).

  localize_region_obj.py --region us --out game_us.o \\
      --whole-archive libp32.a libp33.a --archive libengine.a

Binutils come from the environment, like tools/rename_ovl_sections.py:
DW3_LD / DW3_OBJCOPY / DW3_NM / DW3_OBJDUMP, or DW3_BINUTILS_PREFIX, else bare
names (the PC build).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

SECTION_RE = re.compile(r"^(dw3data|dw3bss)_(.+)$")
START_STOP_RE = re.compile(r"^(__start|__stop)_(dw3data|dw3bss)_(.+)$")


def _tool(name: str) -> str:
    return os.environ.get(f"DW3_{name.upper()}") or (
        os.environ.get("DW3_BINUTILS_PREFIX", "") + name
    )


def _run(cmd: list[str]) -> str:
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def section_names(obj: str) -> list[str]:
    names = []
    for line in _run([_tool("objdump"), "-h", obj]).splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].isdigit():
            names.append(parts[1])
    return names


def symbols(obj: str) -> tuple[dict[str, str], dict[str, str]]:
    """(defined global/weak name -> nm type, undefined name -> nm type)"""
    defined: dict[str, str] = {}
    undefined: dict[str, str] = {}
    for line in _run([_tool("nm"), "-g", "--no-demangle", obj]).splitlines():
        parts = line.split()
        if len(parts) == 2:
            undefined[parts[1]] = parts[0]
        elif len(parts) == 3:
            defined[parts[2]] = parts[1]
    return defined, undefined


def localize(args: argparse.Namespace) -> int:
    region = args.region
    exports = args.export or ["dw3_game_api"]
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(dir=str(out.parent)) as tmp:
        merged = os.path.join(tmp, "merged.o")
        local = os.path.join(tmp, "local.o")
        final = os.path.join(tmp, "final.o")

        ld = [_tool("ld"), "-r", "-o", merged]
        for root in args.root or exports:
            ld += ["-u", root]
        ld += ["--start-group"]
        if args.whole_archive:
            ld += ["--whole-archive", *args.whole_archive, "--no-whole-archive"]
        ld += [*args.archive, *args.obj, "--end-group"]
        subprocess.check_call(ld)

        # Pass 1: only the exports stay global. Undefined symbols stay as they
        # are (they are the host's).
        cmd = [_tool("objcopy")]
        for e in exports:
            cmd.append(f"--keep-global-symbol={e}")
        subprocess.check_call(cmd + [merged, local])

        defined, undefined = symbols(local)
        missing = [e for e in exports if e not in defined]
        if missing:
            print(f"localize_region_obj: export(s) not defined: {missing}", file=sys.stderr)
            return 1
        extra = sorted(set(defined) - set(exports))
        if extra:
            print(f"localize_region_obj: global symbols left over: {extra[:10]}", file=sys.stderr)
            return 1

        # Pass 2: names. Exports get the region prefix; restore sections and the
        # linker-synthesized __start_/__stop_ references get the region infix.
        cmd = [_tool("objcopy")]
        for e in exports:
            cmd.append(f"--redefine-sym={e}={region}_{e}")
        nsec = 0
        for sec in section_names(local):
            m = SECTION_RE.match(sec)
            if m:
                cmd.append(f"--rename-section={sec}={m.group(1)}_{region}_{m.group(2)}")
                nsec += 1
        nref = 0
        for sym in sorted(undefined):
            m = START_STOP_RE.match(sym)
            if m:
                cmd.append(f"--redefine-sym={sym}={m.group(1)}_{m.group(2)}_{region}_{m.group(3)}")
                nref += 1
        subprocess.check_call(cmd + [local, final])

        # Every restore section that exists must have its __start_ reference
        # (overlay_sections.c) and the other way round, or a region would
        # restore nothing / link a stray symbol.
        _, und2 = symbols(final)
        secs2 = {s for s in section_names(final) if SECTION_RE.match(s)}
        wanted = {m.group(0) for m in map(START_STOP_RE.match, und2) if m}
        unknown = sorted(
            s for s in und2
            if (m := START_STOP_RE.match(s)) and f"{m.group(2)}_{m.group(3)}" not in secs2
        )
        if unknown:
            print(f"localize_region_obj: __start_/__stop_ without a section: {unknown[:5]}",
                  file=sys.stderr)
            return 1
        os.replace(final, out)

    print(
        f"localize_region_obj: {region}: {len(exports)} export(s), {nsec} restore "
        f"section(s), {nref} __start_/__stop_ reference(s), {len(wanted)} bound -> {out}",
        file=sys.stderr,
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--region", required=True, help="us or eu (the symbol and section infix)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--export", action="append", help="global symbol to keep (default dw3_game_api)")
    ap.add_argument("--root", action="append", help="symbol that starts the archive pull (default: the exports)")
    ap.add_argument("--whole-archive", nargs="*", default=[], help="archives taken whole")
    ap.add_argument("--archive", nargs="*", default=[], help="archives, members pulled on demand")
    ap.add_argument("--obj", nargs="*", default=[], help="plain objects")
    args = ap.parse_args()
    if not re.fullmatch(r"[a-z0-9]+", args.region):
        ap.error("region must be lowercase alphanumeric")
    return localize(args)


if __name__ == "__main__":
    raise SystemExit(main())
