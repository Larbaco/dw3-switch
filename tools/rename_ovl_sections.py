#!/usr/bin/env python3
"""ld -r game objects then merge writable sections for host overlay restore.

  rename_ovl_sections.py fieldstg out.host.o in1.o in2.o ...
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "tools" / "merge_ovl_sections.ld"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", help="section suffix, e.g. fieldstg")
    ap.add_argument("dst_o")
    ap.add_argument("objects", nargs="+")
    args = ap.parse_args()

    name = args.name.lower()
    dst = Path(args.dst_o)
    dst.parent.mkdir(parents=True, exist_ok=True)
    objs = [str(Path(p)) for p in args.objects]
    script_text = TEMPLATE.read_text().replace("__DW3_OVL__", name)

    with tempfile.TemporaryDirectory(dir=str(dst.parent)) as tmp:
        tmp_path = Path(tmp)
        script = tmp_path / "merge.ld"
        script.write_text(script_text)
        out = tmp_path / "merged.o"
        subprocess.check_call(
            ["ld", "-r", "-T", str(script), "-o", str(out), *objs]
        )
        shutil.copy2(out, dst)

    print(f"rename_ovl_sections: {name} → {dst}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
