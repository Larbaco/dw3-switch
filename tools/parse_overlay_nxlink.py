#!/usr/bin/env python3
"""Parse nxlink / dw3.log lines for P4.6 overlay restore sweeps.

Reads stdin or files. Prints a compact restore sequence and a checklist
summary against the owner OLED path in docs/P4.6_MODULES_STATUS.md.

Does not claim the P4.6 gate closed — it only reports what the log shows.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple

# US mode / battle / menu / card overlays that matter for the OLED checklist.
# Stages share the same restore line shape; they are reported as stage:<hex>.
US_NAMES = {
    0x1C1: "STDWTITL",
    0x158: "FIELDSTG",
    0x1F8: "STPLNMET",
    0x159: "FIGHTSTG",
    0x1FA: "WFIGHTMN",
    0x1C2: "STFGTREP",
    0x1F9: "STSTATUS",
    0x1F7: "STITSHOP",
    0x1F6: "STGTRAIN",
    0x1DF: "STGDGLAB",
    0x1E3: "STGMCARD",
    0x154: "STDGNAME",
    0x153: "CARDGAME",
    0x1BC: "STCRDDEK",
    0x1C0: "STCRDSHP",
    0x177: "STCRDABM",
    0x176: "STAGSLCT",
    0x1CE: "WSTAG260",
    0x1DA: "WSTAG780",
    0x1DB: "WSTAG785",
    0x1DC: "WSTAG790",
}

RESTORE_RE = re.compile(
    r"dw3:\s*overlay restore file=(0x[0-9a-fA-F]+|\d+)"
    r"(?:\s+kind=(?P<kind>mode|sub|unknown))?"
    r"(?P<unknown>\s+UNKNOWN)?"
    r"(?:\s+data=(?P<data>\d+))?"
    r"(?:\s+bss=(?P<bss>\d+))?"
)
REGISTER_RE = re.compile(r"dw3:\s*registered (US|EU) overlays\b.*\bcount=(\d+)")
MODE_RE = re.compile(r"dw3:\s*mode=(0x[0-9a-fA-F]+)")
STAGE_RE = re.compile(r"stageFile=(\d+)")


@dataclass(frozen=True)
class RestoreEvent:
    file_id: int
    kind: str  # mode | sub | unknown | ""
    unknown: bool
    data: Optional[int]
    bss: Optional[int]
    line: str

    @property
    def name(self) -> str:
        return US_NAMES.get(self.file_id, f"file:0x{self.file_id:x}")


# OLED owner checklist steps → required restore names (order flexible within step).
CHECKLIST: Sequence[Tuple[str, Sequence[str]]] = (
    ("boot_register", ()),  # special: registered line
    ("title", ("STDWTITL",)),
    ("new_game_name", ("STPLNMET",)),
    ("field_start", ("FIELDSTG", "WSTAG780")),
    ("map_door", ("WSTAG785",)),  # Online Center from Kusanagi; any new stage also OK
    ("battle_enter", ("FIGHTSTG", "WFIGHTMN")),
    ("battle_report", ("STFGTREP",)),
    ("battle_return_field", ("FIELDSTG",)),
)


def _parse_int(token: str) -> int:
    return int(token, 0)


def parse_lines(lines: Iterable[str]) -> Tuple[List[RestoreEvent], List[str], List[str]]:
    restores: List[RestoreEvent] = []
    registers: List[str] = []
    modes: List[str] = []
    for raw in lines:
        line = raw.rstrip("\n")
        m = RESTORE_RE.search(line)
        if m:
            restores.append(
                RestoreEvent(
                    file_id=_parse_int(m.group(1)),
                    kind=(m.group("kind") or ""),
                    unknown=bool(m.group("unknown")),
                    data=int(m.group("data")) if m.group("data") else None,
                    bss=int(m.group("bss")) if m.group("bss") else None,
                    line=line,
                )
            )
            continue
        if REGISTER_RE.search(line):
            registers.append(line)
        if MODE_RE.search(line):
            modes.append(line)
            # keep scanning; stageFile may share the line
        if STAGE_RE.search(line) and "FIELDSTG_pickStage" in line:
            modes.append(line)
    return restores, registers, modes


def checklist_report(
    restores: Sequence[RestoreEvent], registers: Sequence[str]
) -> List[Tuple[str, str]]:
    """Return (step, status) where status is PASS / MISS / SKIP."""
    names_seen = [ev.name for ev in restores if not ev.unknown]
    unknown = [ev for ev in restores if ev.unknown]
    out: List[Tuple[str, str]] = []

    if registers:
        out.append(("boot_register", "PASS"))
    else:
        out.append(("boot_register", "MISS"))

    # Walk restores in order; each step consumes the next matching name.
    idx = 0
    for step, need in CHECKLIST:
        if step == "boot_register":
            continue
        if not need:
            out.append((step, "SKIP"))
            continue
        found = False
        # Special: map_door accepts any WSTAG* / file: after field_start.
        if step == "map_door":
            while idx < len(names_seen):
                n = names_seen[idx]
                idx += 1
                if n.startswith("WSTAG") or n.startswith("file:"):
                    if n != "WSTAG780":  # new stage after start town
                        found = True
                        break
            out.append((step, "PASS" if found else "MISS"))
            continue
        need_set = set(need)
        hit: set[str] = set()
        while idx < len(names_seen) and hit != need_set:
            n = names_seen[idx]
            idx += 1
            if n in need_set:
                hit.add(n)
            # Allow extra restores (menus, etc.) between required ones.
            if hit == need_set:
                break
        # For multi-name steps, accept if all appeared (order among them flexible
        # within the scan window above).
        out.append((step, "PASS" if hit == need_set else "MISS"))

    if unknown:
        out.append(("no_unknown_restore", f"FAIL({len(unknown)})"))
    else:
        out.append(("no_unknown_restore", "PASS" if restores else "MISS"))
    return out


def format_report(
    restores: Sequence[RestoreEvent],
    registers: Sequence[str],
    modes: Sequence[str],
) -> str:
    lines: List[str] = []
    lines.append(f"registers: {len(registers)}")
    for r in registers:
        lines.append(f"  {r}")
    lines.append(f"restores: {len(restores)}")
    for i, ev in enumerate(restores, 1):
        kind = ev.kind or "?"
        flag = " UNKNOWN" if ev.unknown else ""
        lines.append(
            f"  {i:3d}. 0x{ev.file_id:03x} {ev.name:12s} kind={kind}{flag}"
        )
    steps = checklist_report(restores, registers)
    lines.append("checklist:")
    for step, status in steps:
        lines.append(f"  {step:22s} {status}")
    miss = [
        s for s, st in steps if st.startswith("MISS") or st.startswith("FAIL")
    ]
    if miss:
        lines.append(
            "result: INCOMPLETE — paste this under docs/P4.6 evidence; gate stays open"
        )
    elif restores:
        lines.append(
            "result: LOG COVERS CHECKLIST STEPS — owner still confirms play feel; "
            "do not tick ROADMAP P4.6 without OLED session notes"
        )
    else:
        lines.append("result: NO RESTORE LINES")
    if modes:
        lines.append(f"mode/stage lines (sample up to 12): {len(modes)}")
        for m in modes[:12]:
            lines.append(f"  {m}")
    return "\n".join(lines) + "\n"


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="nxlink capture / dw3.log files (default: stdin)",
    )
    args = ap.parse_args(argv)

    if args.paths:
        chunks: List[str] = []
        for p in args.paths:
            chunks.extend(p.read_text(encoding="utf-8", errors="replace").splitlines())
        lines = chunks
    else:
        lines = sys.stdin.read().splitlines()

    restores, registers, modes = parse_lines(lines)
    sys.stdout.write(format_report(restores, registers, modes))
    # Exit 0 even when incomplete — this is a report tool, not a CI gate.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
