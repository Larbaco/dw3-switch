"""Every markdown file in the repo (outside vendor/) has working relative
links and anchors, and docs/README.md lists every doc.

Runs tools/check_doc_links.py in-process; no disc, no build, well under a second.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import check_doc_links as cdl  # noqa: E402

SKIP_TOP = {"vendor", "build", "node_modules", "tmp"}


def markdown_files() -> list:
    out = []
    for p in sorted(ROOT.rglob("*.md")):
        rel = p.relative_to(ROOT)
        top = rel.parts[0]
        if top in SKIP_TOP or top.startswith("."):
            continue
        out.append(p)
    return out


def test_markdown_files_found():
    names = {p.relative_to(ROOT).as_posix() for p in markdown_files()}
    assert {"README.md", "AGENTS.md", "CLAUDE.md", "CONTRIBUTING.md",
            "docs/README.md"} <= names


def test_no_broken_links_or_anchors():
    errors = []
    for p in markdown_files():
        errors += cdl.check(p)
    assert not errors, "\n".join(errors)


def test_docs_index_lists_every_doc():
    assert not cdl.check_index()


def test_slug_keeps_underscores():
    assert cdl.slug("Empty drawing area (sdl3_gpu)") == "empty-drawing-area-sdl3_gpu"
