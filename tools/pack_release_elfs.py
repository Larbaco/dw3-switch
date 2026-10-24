#!/usr/bin/env python3
"""Pack the unstripped .elf of each released .nro into one zip (P5.4).

tools/symbolize_crash.py needs the exact .elf of the build that crashed, and
a rebuild does not reproduce the build id. The release workflow runs this so
the .elf files are published next to the .nro files.

  tools/pack_release_elfs.py --out dist/dw3-switch-elf.zip --tag v0.6.5-preview.31 \\
      --item dist/dw3-switch.nro build/switch/dw3-switch.elf dw3-switch-us.elf \\
      --item dist/dw3-switch-nxlink.nro build/switch-nxlink/dw3-switch.elf dw3-switch-us-nxlink.elf

Each --item is NRO ELF NAME: the .nro the .elf belongs to, the .elf on disk,
and its file name inside the zip. The GNU build id of the .elf (read by
symbolize_crash.elf_build_id, the same code that matches a crash report) must
equal the module id in the .nro header, or this exits 1. The zip also holds
BUILD_IDS.txt (build id, .elf and .nro per line). Nothing is stripped.
"""
import argparse
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from symbolize_crash import elf_build_id  # noqa: E402

NRO_MODULE_ID_OFF = 0x40  # NRO0 header: 32-byte module id (build id, zero padded)
NRO_MODULE_ID_LEN = 0x20


def nro_build_id(path: Path) -> str:
    head = path.read_bytes()[:NRO_MODULE_ID_OFF + NRO_MODULE_ID_LEN + 4]
    if head[0x10:0x14] != b"NRO0":
        raise SystemExit(f"{path} is not an NRO (no NRO0 magic)")
    mid = head[NRO_MODULE_ID_OFF:NRO_MODULE_ID_OFF + NRO_MODULE_ID_LEN]
    return mid.rstrip(b"\0").hex()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--tag", default="", help="release tag, for BUILD_IDS.txt")
    ap.add_argument("--item", nargs=3, action="append", required=True,
                    metavar=("NRO", "ELF", "NAME"))
    a = ap.parse_args()

    lines = []
    if a.tag:
        lines.append(f"release: {a.tag}")
    lines += [
        "Unstripped .elf files for symbolizing crash reports with",
        "tools/symbolize_crash.py (docs/P5.4_CRASH.md). Match the crash_report.txt",
        "'build_id:' line to one of the ids below, then:",
        "  python3 tools/symbolize_crash.py crash_report.txt <elf>",
        "",
        "build_id                                  elf                          nro",
    ]
    entries = []
    for nro, elf, name in a.item:
        nro, elf = Path(nro), Path(elf)
        bid = elf_build_id(elf)
        if not bid:
            print(f"{elf} has no GNU build-id", file=sys.stderr)
            return 1
        nid = nro_build_id(nro)
        if nid != bid:
            print(f"{elf} build-id {bid} != {nro.name} module id {nid}", file=sys.stderr)
            return 1
        lines.append(f"{bid}  {name:<28} {nro.name}")
        entries.append((elf, name))
        print(f"{name}: build_id {bid} matches {nro.name} ({elf.stat().st_size} bytes)")

    a.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr("BUILD_IDS.txt", "\n".join(lines) + "\n")
        for elf, name in entries:
            z.write(elf, name)
    print(f"wrote {a.out} ({a.out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
