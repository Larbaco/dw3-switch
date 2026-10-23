"""Keeps port/patches/ and the Series table in docs/PATCHES.md in step.

- A patch number is used by one file, except the historical 0051 pair.
- Every patch file has a row in the Series table, and every row names a
  file that exists.
"""

from __future__ import annotations

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATCH_DIR = ROOT / "port" / "patches"
PATCHES_MD = ROOT / "docs" / "PATCHES.md"

# Two files were numbered 0051 before the rule existed. They are not renamed
# (other docs and history refer to them). Do not add to this list.
DUPLICATE_ALLOW_LIST = {
    "0051": {
        "0051-PORT_HOST-p61-widescreen-fades.patch",
        "0051-PORT_HOST-p64-fast-forward-poll.patch",
    },
}


def patch_files() -> list[str]:
    return sorted(p.name for p in PATCH_DIR.glob("*.patch"))


def series_table_files() -> list[str]:
    text = PATCHES_MD.read_text(encoding="utf-8")
    start = text.index("\n## Series")
    end = text.find("\n##", start + 1)
    section = text[start:end if end != -1 else len(text)]
    return re.findall(r"^\| `(\d{4}-[^`]+\.patch)` \|", section, re.MULTILINE)


def test_patch_numbers_are_unique():
    by_number: dict[str, set[str]] = defaultdict(set)
    for name in patch_files():
        by_number[name[:4]].add(name)
    dupes = {n: files for n, files in by_number.items() if len(files) > 1}
    unexpected = {
        n: sorted(files)
        for n, files in dupes.items()
        if files != DUPLICATE_ALLOW_LIST.get(n)
    }
    assert not unexpected, (
        "duplicate patch numbers (take the next free number): "
        f"{unexpected}"
    )


def test_allow_listed_duplicates_still_exist():
    """Fail when the allow-list goes stale, so it cannot hide a new pair."""
    names = set(patch_files())
    for number, files in DUPLICATE_ALLOW_LIST.items():
        assert files <= names, (
            f"allow-listed duplicates for {number} are gone; "
            "remove the entry from DUPLICATE_ALLOW_LIST"
        )


def test_every_patch_is_in_the_series_table():
    listed = series_table_files()
    missing = sorted(set(patch_files()) - set(listed))
    assert not missing, f"not in the docs/PATCHES.md Series table: {missing}"


def test_series_table_names_existing_patches():
    unknown = sorted(set(series_table_files()) - set(patch_files()))
    assert not unknown, f"Series table rows without a patch file: {unknown}"


def test_series_table_has_no_repeated_rows():
    listed = series_table_files()
    repeated = sorted({n for n in listed if listed.count(n) > 1})
    assert not repeated, f"repeated Series table rows: {repeated}"


def test_series_table_follows_apply_order():
    """apply_patches.sh applies in `LC_ALL=C sort` order; the table matches."""
    listed = series_table_files()
    assert listed == sorted(listed), "Series table rows are not in apply order"
