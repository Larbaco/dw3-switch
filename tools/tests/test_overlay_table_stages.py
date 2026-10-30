"""The host's overlay tables cover every stage the decomp defines.

port/overlays/overlay_table_{us,eu}.c are written by hand, and a stage is
registered in several places that must agree (the file id, the enum slot and
its row, the name, the section binding, the CMake stage list). A stage that is
missing from them loads as "file id not in the overlay table": strict replay
aborts, and the game crashes on its first frame. WSTAG460 (mode 0x236, the
Reliability Spot, on the main story path at progress 0xE) was missing for that
reason until patch 0197; the European extra chapter's 55 stages (WSTAG920 to
WSTAG974) were too. docs/P5.5_STAGE_SWEEP.md.

The reference is the decomp itself: config/<region>/stages.txt (every stage
file of the disc) and FIELDSTG_stages / FIELDSTG_extraStages (the file id of
each stage and the mode that starts it). No stage is excepted.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor" / "dw3_decomp"
FIELDSTG = DECOMP / "src/field/field_mode/data/fieldstg.c"
OVL = ROOT / "port" / "overlays"
CMAKE = ROOT / "port" / "CMakeLists.txt"

pytestmark = pytest.mark.skipif(not FIELDSTG.exists(), reason="decomp submodule not checked out")

ENTRY = re.compile(r"\{\s*(\d+),\s*(\d+),\s*WSTAG(\d+)_startStage\s*\}")


def _tables() -> dict[str, dict[int, int]]:
    """{table name: {WSTAG number: file id}}; FIELDSTG_stages is defined twice,
    the US one first and the EU one second (tools/gen_sweep_stages.py)."""
    text = FIELDSTG.read_text()
    out: dict[str, dict[int, int]] = {}
    stages = re.findall(r"StageEntry FIELDSTG_stages\[\] = \{(.*?)\n\};", text, re.S)
    assert len(stages) == 2
    for name, body in (("us", stages[0]), ("eu", stages[1])):
        out[name] = {int(n): int(f) for _m, f, n in ENTRY.findall(body)}
    extra = re.search(r"StageEntry FIELDSTG_extraStages\[\] = \{(.*?)\n\};", text, re.S)
    assert extra is not None
    out["extra"] = {int(n): int(f) for _m, f, n in ENTRY.findall(extra.group(1))}
    return out


def _config(region: str) -> set[int]:
    names = set()
    for line in (DECOMP / "config" / region / "stages.txt").read_text().splitlines():
        m = re.match(r"WSTAG(\d+)\s", line)
        if m:
            names.add(int(m.group(1)))
    return names


def _table_text(region: str) -> str:
    return (OVL / f"overlay_table_{region}.c").read_text()


def _sections_text(region: str) -> str:
    text = (OVL / "overlay_sections.c").read_text()
    if region == "eu":
        text += (OVL / "overlay_sections_eu_extra.c").read_text()
    return text


def _expected(region: str) -> dict[int, int | None]:
    """{WSTAG number: file id the table must define (None: not in FIELDSTG)}."""
    t = _tables()
    exp: dict[int, int | None] = {n: None for n in _config(region)}
    exp.update(t[region])
    if region == "eu":
        exp.update(t["extra"])
    return exp


def test_decomp_tables_are_what_the_test_assumes():
    t = _tables()
    assert len(t["us"]) == 238 and len(t["eu"]) == 238
    assert sorted(t["extra"]) == list(range(920, 975))
    # stages.txt names every stage file: FIELDSTG's plus WSTAG260 (the first
    # stage, started by the new-game script, not listed in FIELDSTG_stages)
    assert _config("us") == set(t["us"]) | {260}
    assert _config("eu") == set(t["eu"]) | set(t["extra"]) | {260}


@pytest.mark.parametrize("region", ["us", "eu"])
def test_every_decomp_stage_is_in_the_overlay_table(region):
    text = _table_text(region)
    sections = _sections_text(region)
    cmake = CMAKE.read_text()
    linked = re.search(r"set\(DW3_LINKED_STAGES\s+(\S+)\s*\)", cmake)
    assert linked is not None
    # the EU game appends the extra chapter's stages to the list
    linked_text = ",".join([linked.group(1)] + re.findall(r'string\(APPEND DW3_LINKED_STAGES ",([^"]+)"\)', cmake))
    pfx = f"g_{region}_overlays"
    problems = []
    for num, fid in sorted(_expected(region).items()):
        n = f"WSTAG{num}"
        m = re.search(rf"^#define DW3_FILE_{n} (0x[0-9A-Fa-f]+)\b", text, re.M)
        if m is None:
            problems.append(f"{n}: no DW3_FILE_{n} define")
        elif fid is not None and int(m.group(1), 16) != fid:
            problems.append(f"{n}: file id {m.group(1)} but the decomp says {fid}")
        for what, pat in (
            ("enum slot", rf"^    OVL_{n},$"),
            ("name", rf'^    \[OVL_{n}\] = "{n}",$'),
            ("descriptor row", rf"^    \{{DW3_FILE_{n}, NULL, NULL, NULL, 0, NULL, 0\}},$"),
            ("entry reset", rf"^    {pfx}\[OVL_{n}\]\.entry = NULL;$"),
        ):
            if re.search(pat, text, re.M) is None:
                problems.append(f"{n}: no {what}")
        low = n.lower()
        if f"__start_dw3data_{low}, __stop_dw3data_{low}" not in sections:
            problems.append(f"{n}: its .data/.bss sections are not bound")
        if not re.search(rf"\b{n}\b", linked_text):
            problems.append(f"{n}: not in DW3_LINKED_STAGES (a weak stub would be linked)")
        if not re.search(rf'dw3_host_stage\({low} "', cmake):
            problems.append(f"{n}: no dw3_host_stage in port/CMakeLists.txt")
        if f"{low}.host.o" not in cmake:
            problems.append(f"{n}: its host object is not in DW3_P32_HOST_OBJECTS")
    assert not problems, "\n".join(problems)


@pytest.mark.parametrize("region", ["us", "eu"])
def test_enum_order_is_the_descriptor_order(region):
    """g_<region>_overlays is positional: row i belongs to enum slot i."""
    text = _table_text(region)
    enum = re.search(r"enum \{(.*?)OVL_COUNT", text, re.S)
    assert enum is not None
    slots = re.findall(r"^\s+OVL_(\w+?)(?: = 0)?,$", enum.group(1), re.M)
    table = re.search(rf"g_{region}_overlays\[OVL_COUNT\] = \{{(.*?)\n\}};", text, re.S)
    assert table is not None
    rows = re.findall(r"^\s+\{DW3_FILE_(\w+),", table.group(1), re.M)
    assert rows == slots


@pytest.mark.parametrize("region", ["us", "eu"])
def test_stage_file_ids_are_unique(region):
    text = _table_text(region)
    stage_ids: dict[int, str] = {}
    for name, value in re.findall(r"^#define DW3_FILE_(WSTAG\d+) (0x[0-9A-Fa-f]+)\b", text, re.M):
        v = int(value, 16)
        assert v not in stage_ids, f"{name} and {stage_ids[v]} share file id {value}"
        stage_ids[v] = name


def test_extra_chapter_is_eu_only():
    """The US game has no WSTAG920-974 (they use LANGUAGE), the EU game has all 55."""
    us = _table_text("us")
    eu = _table_text("eu")
    for num in range(920, 975):
        assert f"DW3_FILE_WSTAG{num} " not in us
        assert f"DW3_FILE_WSTAG{num} " in eu
