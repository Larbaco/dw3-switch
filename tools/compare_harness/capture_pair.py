#!/usr/bin/env python3
"""Capture DuckStation vs dw3-pc frames for the same save/label (P3.9)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

LABELS = ("title", "field", "battle")


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=(
            "Capture DuckStation vs dw3-pc frames for the same save/label "
            "(P3.9). Use --dry-run to validate args without assets."
        )
    )
    p.add_argument(
        "--label",
        required=True,
        choices=LABELS,
        help="scene label: title, field, or battle",
    )
    p.add_argument(
        "--runtime",
        type=Path,
        default=Path("runtime/us"),
        help="PC runtime data root (default: runtime/us)",
    )
    p.add_argument(
        "--save",
        type=Path,
        default=None,
        help="shared slot-0 .mcr path (optional for --dry-run)",
    )
    p.add_argument(
        "--duckstation",
        type=Path,
        default=None,
        help="DuckStation executable (owner path; unused in --dry-run)",
    )
    p.add_argument(
        "--pc-bin",
        type=Path,
        default=Path("build/pc/dw3-pc"),
        help="dw3-pc binary (default: build/pc/dw3-pc)",
    )
    p.add_argument(
        "--out",
        type=Path,
        default=Path("build/compare"),
        help="output root for <label>/{duck,pc}.png (default: build/compare)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="print capture plan and exit 0; no disc/emulator/binary required",
    )
    return p


def print_plan(args: argparse.Namespace) -> None:
    save = args.save if args.save is not None else Path("<unset>")
    out_dir = args.out / args.label
    print(
        f"compare_harness: stub label={args.label} runtime={args.runtime}",
        file=sys.stderr,
    )
    print(f"  save (.mcr): {save}", file=sys.stderr)
    print(f"  duckstation: {args.duckstation or '<unset>'}", file=sys.stderr)
    print(f"  pc-bin:      {args.pc_bin}", file=sys.stderr)
    print(f"  out:         {out_dir}/{{duck,pc}}.png + pair.md", file=sys.stderr)
    print(
        "  next: implement oracle/subject capture; see tools/compare_harness/README.md",
        file=sys.stderr,
    )


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.dry_run:
        print_plan(args)
        return 0
    print(
        "compare_harness: capture not implemented yet; use --dry-run "
        "(see tools/compare_harness/README.md)",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
