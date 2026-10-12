"""Unit tests for tools/parse_overlay_nxlink.py (P4.6 OLED log harness)."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MOD_PATH = ROOT / "tools" / "parse_overlay_nxlink.py"


def _load():
    name = "parse_overlay_nxlink"
    spec = importlib.util.spec_from_file_location(name, MOD_PATH)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    # dataclasses need the module in sys.modules while the class body runs
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


SAMPLE = """
dw3: registered US overlays STDWTITL/FIELDSTG/STPLNMET/ count=255
dw3: overlay restore file=0x1c1 kind=mode data=100 bss=20 entry=0x1
dw3: mode=0xe00 title_entry=0x1
dw3: overlay restore file=0x1f8 kind=mode data=50 bss=10 entry=0x2
dw3: overlay restore file=0x158 kind=mode data=200 bss=40 entry=0x3
dw3: overlay restore file=0x1da kind=sub data=80 bss=8 entry=(nil)
dw3: FIELDSTG_pickStage mode=0x2d7 stageFile=474
dw3: overlay restore file=0x1db kind=sub data=80 bss=8 entry=(nil)
dw3: overlay restore file=0x159 kind=mode data=300 bss=50 entry=0x4
dw3: overlay restore file=0x1fa kind=sub data=90 bss=9 entry=(nil)
dw3: overlay restore file=0x1c2 kind=mode data=40 bss=4 entry=0x5
dw3: overlay restore file=0x158 kind=mode data=200 bss=40 entry=0x3
"""


def test_parse_full_oled_path():
    mod = _load()
    restores, registers, modes = mod.parse_lines(SAMPLE.splitlines())
    assert len(registers) == 1
    assert [r.name for r in restores] == [
        "STDWTITL",
        "STPLNMET",
        "FIELDSTG",
        "WSTAG780",
        "WSTAG785",
        "FIGHTSTG",
        "WFIGHTMN",
        "STFGTREP",
        "FIELDSTG",
    ]
    assert restores[0].kind == "mode"
    assert restores[3].kind == "sub"
    report = mod.checklist_report(restores, registers)
    by_step = dict(report)
    assert by_step["boot_register"] == "PASS"
    assert by_step["title"] == "PASS"
    assert by_step["new_game_name"] == "PASS"
    assert by_step["field_start"] == "PASS"
    assert by_step["map_door"] == "PASS"
    assert by_step["battle_enter"] == "PASS"
    assert by_step["battle_report"] == "PASS"
    assert by_step["battle_return_field"] == "PASS"
    assert by_step["no_unknown_restore"] == "PASS"
    assert modes  # mode / pickStage lines retained
    text = mod.format_report(restores, registers, modes)
    assert "LOG COVERS CHECKLIST STEPS" in text


def test_unknown_restore_flagged():
    mod = _load()
    restores, registers, _ = mod.parse_lines(
        [
            "dw3: registered US overlays count=1",
            "dw3: overlay restore file=0x999 kind=unknown UNKNOWN",
        ]
    )
    by_step = dict(mod.checklist_report(restores, registers))
    assert by_step["no_unknown_restore"].startswith("FAIL")


def test_legacy_restore_line_without_kind():
    """Older builds omitted kind=; still parse file id."""
    mod = _load()
    restores, _, _ = mod.parse_lines(
        ["dw3: overlay restore file=0x158 data=1 bss=2 entry=0x1"]
    )
    assert len(restores) == 1
    assert restores[0].file_id == 0x158
    assert restores[0].name == "FIELDSTG"
    assert restores[0].kind == ""
