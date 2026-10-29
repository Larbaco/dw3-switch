#!/usr/bin/env python3
"""Release-prune helper for the "OLED checklist" in the release notes (#323 rule).

docs/RELEASES.md: before a release, prune the "still open" lists in the notes
against the PASSED marks in docs/OLED_TEST_PLAN.md. This script does the
bookkeeping:

  tools/oled_open_rows.py              list the plan rows without a PASSED mark
  tools/oled_open_rows.py --notes F    also warn about row IDs that the OLED
                                       checklist of notes file F names while
                                       the plan marks them PASSED (no PARTIAL)

Warnings do not fail the run: a row can be PASSED for an old behaviour and
still need a re-check (say so in the notes). Exit status is 0 unless the plan
or the notes file cannot be read.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "docs" / "OLED_TEST_PLAN.md"

ROW_ID = re.compile(r"^(?:[A-Z]\d+(?:\.\d+[a-z]?)?|H\d+|Item)$")
ID_IN_TEXT = re.compile(r"\b([A-H]\d+(?:\.\d+[a-z]?)?)\b")


def parse_plan(text: str) -> list[dict]:
    """Table rows of the plan: id, status ('PASSED', 'PARTIAL', 'open'), session."""
    rows = []
    session = ""
    for line in text.splitlines():
        if line.startswith("## "):
            session = line[3:].strip()
            continue
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not cells or not ROW_ID.match(cells[0]):
            continue
        row = "|".join(cells)
        if "PASSED" in row:
            status = "PASSED"
        else:
            status = "open"
        if "PARTIAL" in row:
            status += "+PARTIAL"
        rows.append({"id": cells[0], "status": status, "session": session,
                     "action": cells[1] if len(cells) > 1 else ""})
    return rows


def checklist_section(notes: str) -> str:
    """The text of the "OLED checklist" block of a release notes file."""
    start = notes.find("OLED checklist")
    if start < 0:
        return ""
    end = notes.find("After any crash", start)
    return notes[start:end if end > 0 else len(notes)]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--plan", type=Path, default=PLAN)
    ap.add_argument("--notes", type=Path, help="release notes file to check")
    a = ap.parse_args(argv)
    try:
        rows = parse_plan(a.plan.read_text(encoding="utf-8"))
        notes = a.notes.read_text(encoding="utf-8") if a.notes else ""
    except OSError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    open_rows = [r for r in rows if "PASSED" not in r["status"] or "PARTIAL" in r["status"]]
    print(f"plan rows without a clean PASSED mark: {len(open_rows)} of {len(rows)}")
    for r in open_rows:
        print(f"  {r['id']:<8} [{r['status']}] {r['action'][:90]}")

    if a.notes:
        passed = {r["id"] for r in rows if r["status"] == "PASSED"}
        section = checklist_section(notes)
        if not section:
            print(f"warning: no 'OLED checklist' section in {a.notes}")
        named = sorted(set(ID_IN_TEXT.findall(section)) & passed)
        for rid in named:
            print(f"warning: {rid} is PASSED in the plan but named in the OLED checklist")
    return 0


if __name__ == "__main__":
    sys.exit(main())
