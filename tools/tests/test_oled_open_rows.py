"""tools/oled_open_rows.py: plan parsing and PASSED warnings for release notes."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("oled_open_rows", ROOT / "tools" / "oled_open_rows.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

PLAN = """\
## Session A

| # | Action | Expect | Ticks |
|---|---|---|---|
| A1 | Launch | ok | boot |
| A5 | Options | ok | **PASSED**: owner log |
| A7 | Quit | ok | **PARTIAL**: saves not reported |
| A8 | Relaunch | ok | later |
"""


def test_parse_status_and_session():
    rows = mod.parse_plan(PLAN)
    got = {r["id"]: r["status"] for r in rows}
    assert got == {"A1": "open", "A5": "PASSED", "A7": "open+PARTIAL", "A8": "open"}
    assert all(r["session"] == "Session A" for r in rows)


def test_checklist_section_ignores_a_sentence_that_mentions_it():
    notes = ("The OLED checklist at the end lists A5.\n\nOLED checklist\n  - A8 item\n\n"
             "  After any crash, keep\n")
    got = mod.checklist_section(notes)
    assert got.startswith("OLED checklist\n")
    assert "A5" not in got
    assert "A8 item" in got


def test_checklist_section_stops_before_crash_paragraph():
    notes = "intro\nOLED checklist\n  - A5 item\n\n  After any crash, keep\nend A1"
    assert mod.checklist_section(notes).strip() == "OLED checklist\n  - A5 item"


def test_main_warns_only_for_clean_passed_rows(tmp_path, capsys):
    plan = tmp_path / "plan.md"
    plan.write_text(PLAN, encoding="utf-8")
    notes = tmp_path / "notes.txt"
    notes.write_text("OLED checklist\n  - A5 and A7 and A8\n\n  After any crash, x\n", encoding="utf-8")
    assert mod.main(["--plan", str(plan), "--notes", str(notes)]) == 0
    out = capsys.readouterr().out
    assert "warning: A5 is PASSED" in out
    assert "warning: A7" not in out  # PASSED+PARTIAL is still open
    assert "warning: A8" not in out
