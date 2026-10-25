#!/usr/bin/env python3
"""Check relative links and anchors in markdown files, and that docs/README.md
lists every doc in docs/.

Usage: python3 tools/check_doc_links.py [file.md ...]
With no arguments it checks README.md, docs/README.md and docs/PLAYER_GUIDE.md.
Exit status 1 when a link, an anchor or an index entry is missing.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT = ["README.md", "docs/README.md", "docs/PLAYER_GUIDE.md"]
LINK = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)\)")


def slug(heading: str) -> str:
    """GitHub-style anchor: lower case, drop punctuation, spaces to dashes."""
    text = re.sub(r"[`*_]", "", heading.strip().lower())
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def anchors(path: Path) -> set:
    found, in_code = set(), False
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("```"):
            in_code = not in_code
        elif not in_code and line.startswith("#"):
            found.add(slug(line.lstrip("#")))
    return found


def check(path: Path) -> list:
    errors, in_code = [], False
    for no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue
        for target in LINK.findall(line):
            if re.match(r"[a-z][a-z0-9+.-]*:", target):
                continue  # http, mailto, ...
            name, _, frag = target.partition("#")
            dest = path if not name else (path.parent / name).resolve()
            if not dest.exists():
                errors.append(f"{path}:{no}: missing file {target}")
            elif frag and dest.suffix == ".md" and frag not in anchors(dest):
                errors.append(f"{path}:{no}: missing anchor {target}")
    return errors


def check_index() -> list:
    index = ROOT / "docs" / "README.md"
    text = index.read_text(encoding="utf-8")
    return [f"docs/README.md: {p.name} is not listed"
            for p in sorted((ROOT / "docs").glob("*.md"))
            if p.name != "README.md" and p.stem not in text]


def main() -> int:
    files = [ROOT / a for a in (sys.argv[1:] or DEFAULT)]
    errors = []
    for f in files:
        errors += check(f)
    if not sys.argv[1:]:
        errors += check_index()
    for e in errors:
        print(e)
    print(f"{len(files)} file(s) checked, {len(errors)} problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
