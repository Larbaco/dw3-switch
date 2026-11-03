"""P6.7: tools/l10n/screens.py and fit.py, the box a dialogue string is shown in and whether a line
fits it. Synthetic text and a fake decomp only: no game text, no runtime."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "l10n"))
sys.path.insert(0, str(ROOT / "tools" / "tests"))

import fit as F  # noqa: E402
import screens as S  # noqa: E402
import textfmt as T  # noqa: E402
from test_l10n_extract import write_fake_decomp  # noqa: E402

EU = T.codec_for("eu")


def enc(text: str) -> bytes:
    return T.encode(text, EU)


def fake_metrics(tmp_path: Path) -> dict[int, T.Metrics]:
    # EU letters are 7 px, accents and the space (icons) 9 px
    return T.load_metrics(write_fake_decomp(tmp_path / "decomp"), "eu")


def write_stage(root: Path, name: str, body: str) -> None:
    d = root / "src" / "field" / "stages" / "south_sector"
    d.mkdir(parents=True, exist_ok=True)
    (d / name).write_text(body)


STAGE = """
#define STAGE_TEXT TEXT_FILE(TEXT_TALKS_06)
#define EVENT_TEXT_FILE (TEXT_EVENTS_700 - 1)
s16 script5[] = {
    SCRIPT_WALK(2, 0xA0, 0x148, 3),
    SCRIPT_TALK(0, 1, 0x15, 0),
    SCRIPT_TALK(0, 2, 0x15, 3),
    SCRIPT_TALK(1, 3, 0, 4),
    SCRIPT_END,
};
FieldTalk actor0Talks[] = {
    { actor0Talk0Conditions, actor0Talk0Actions, 0x16A },
    { NULL, NULL, 7 },
    { NULL, NULL, 0 },
};
FieldEvent stageEvents[] = {
    { 5, script5, EVENT_TEXT(0x12), NULL, NULL },
    { -1, NULL, 0, NULL, NULL },
};
"""


def test_screens_say_which_box_shows_a_string(tmp_path):
    write_stage(tmp_path, "wstag999.c", STAGE)
    c = S.classify(tmp_path)
    # SCRIPT_TALK kind 0..3 is a talk box at a corner, kind 4 the message box
    assert c["DMG700:18:1"] == {S.TALK_BOX}
    assert c["DMG700:18:2"] == {S.TALK_BOX}
    assert c["DMG700:18:3"] == {S.MESSAGE_BOX}
    # an actor's FieldTalk entry is always a talk box (FIELDSTG_createTalk); the 0 terminator is not a string
    assert c["TALK06:362"] == {S.TALK_BOX} and c["TALK06:7"] == {S.TALK_BOX}
    assert "TALK06:0" not in c


def test_the_speaker_is_not_a_line_of_the_box(tmp_path):
    st = fake_metrics(tmp_path)[1]
    with_name = enc("{start}Abcdefghijkl{start}Hello\nWorld{wait:2}")
    plain = enc("Hello\nWorld{wait:2}")
    assert T.line_widths(with_name, st)[0] > T.line_widths(plain, st)[0]  # the old measure counted the name
    assert F.body_widths(with_name, st) == F.body_widths(plain, st) == [35, 35]
    assert F.speaker_split(with_name)[1] == plain
    assert F.speaker_split(plain) == (b"", plain)


def test_boxes_fall_back_to_the_file_and_to_the_english_width(tmp_path):
    st = fake_metrics(tmp_path)[1]
    write_stage(tmp_path, "wstag999.c", STAGE)
    boxes = F.Boxes(tmp_path)
    assert boxes.box("TALK03:9") == (F.TALK, "file")  # TALK files are only ever shown by createTalk
    assert boxes.box("DMG700:18:3") == (F.MESSAGE, "code")
    narrow, wide = enc("a" * 10), enc("a" * 30)  # 70 px and 210 px
    assert boxes.box("DMG400:2:2", narrow, st) == (F.TALK, "inferred")
    assert boxes.box("DMG400:2:2", wide, st) == (F.MESSAGE, "inferred")
    assert boxes.box("DMG400:2:2") == (None, "")
    assert boxes.box("ITMNAM:3") == (None, "")  # no box of ours: it is a menu window


def test_overflow_uses_the_box_and_spares_what_the_english_does(tmp_path):
    st = fake_metrics(tmp_path)[1]
    ok, wide = enc("a" * 21), enc("a" * 22)  # 147 and 154 px: the talk box holds 148
    assert F.overflow(ok, F.TALK, st) == []
    assert F.overflow(wide, F.TALK, st) == [154]
    assert F.overflow(wide, F.TALK, st, en_raw=wide) == []  # the original is as wide: not our flaw
    assert F.overflow(wide, F.MESSAGE, st) == []  # the same line fits the message box (252)
    assert F.overflow(enc("a" * 37), F.MESSAGE, st) == [259]


def test_autofit_rewraps_only_the_page_that_overflows(tmp_path):
    ms = fake_metrics(tmp_path)
    st, codec = ms[1], EU
    page1 = "{start}Mia{start}one two three four five six seven\neight nine ten eleven{wait:2}"
    page2 = "short\nlines here{wait:2}"
    text = page1 + "{pg}" + page2
    assert F.overflow(enc(text), F.TALK, st)
    new, status, why = F.autofit(text, F.TALK, codec, st, None, ms)
    assert status == "rewrapped" and why == ""
    assert new.endswith("{pg}" + page2) and new.startswith("{start}Mia{start}")
    raw = enc(new)
    assert F.overflow(raw, F.TALK, st) == [] and max(T.page_lines(F.speaker_split(raw)[1])) <= 3
    assert T.tokens(new) == T.tokens(text)  # the same codes, in the same order
    assert new.replace("\n", " ") == text.replace("\n", " ")  # no word changed
    again = F.autofit(new, F.TALK, codec, st, None, ms)
    assert again == (new, "ok", "")  # idempotent


def test_autofit_prefers_the_box_width_then_the_frame(tmp_path):
    ms = fake_metrics(tmp_path)
    st = ms[1]
    # 19 + 1 + 19 letters: one line of 7*39 + 9 = 282 px, two lines of at most 139 px are possible
    new, status, _ = F.autofit("a" * 19 + " " + "b" * 19 + "{wait:2}", F.TALK, EU, st, None, ms)
    assert status == "rewrapped" and new == "a" * 19 + "\n" + "b" * 19 + "{wait:2}"
    assert max(F.body_widths(enc(new), st)) <= F.TALK_TARGET_PX
    # one word of 141 px: over the 139 target and inside the 148 frame, left alone (no overflow)
    assert F.autofit("a" * 20, F.TALK, EU, st, None, ms)[1] == "ok"


def test_autofit_says_what_it_cannot_fit(tmp_path):
    ms = fake_metrics(tmp_path)
    st = ms[1]
    long = " ".join(["word"] * 20)  # 20 words of 28 px, far more than 3 lines of 148 px
    assert F.autofit(long, F.TALK, EU, st, None, ms) == (long, "unfit", f"does not fit in 3 lines of 148 px")
    one = "a" * 30  # a single word wider than the box
    new, status, why = F.autofit(one, F.TALK, EU, st, None, ms)
    assert (new, status, why) == (one, "unfit", "one word is wider than the box")
    mid = "one two{wait:2}three four five six seven eight nine ten eleven"
    assert F.autofit(mid, F.TALK, EU, st, None, ms)[1:] == ("unfit", "a wait, pause or raw code in the middle of the page")


def test_autofit_keeps_the_name_bar_wide_enough(tmp_path):
    ms = fake_metrics(tmp_path)
    st = ms[1]
    # the box is as wide as its widest line, so a tighter wrap narrows the speaker's bar. A name of
    # 126 px sits in the bar of a 139 px box (149 with its margin) but not in one of 95 px (105)
    body = " ".join(["abcdef"] * 6)  # words of 42 px: 2 fit a 139 px line (93), 3 fit the 148 px frame (144)
    text = "{start}" + "n" * 18 + "{start}" + body
    assert F.overflow(enc(text), F.TALK, st) and F.speaker_overflow(enc(text), ms) == 0
    new, status, _ = F.autofit(text, F.TALK, EU, st, None, ms)
    assert status == "rewrapped"
    assert F.speaker_overflow(enc(new), ms) == 0  # the wider wrap (3 words a line) was used
    assert max(F.body_widths(enc(new), st)) == 144
    # with no styles to measure the name, the tighter wrap wins
    tight = F.autofit(text, F.TALK, EU, st, None, None)[0]
    assert max(F.body_widths(enc(tight), st)) <= F.TALK_TARGET_PX


def test_apply_autofit_does_not_touch_other_files_or_the_input(tmp_path):
    decomp = write_fake_decomp(tmp_path / "decomp")
    wide = "{start}Mia{start}" + " ".join(["abcd"] * 9) + "{wait:2}"
    tpls = {
        "TALK01": {"stem": "TALK01", "entries": [{"key": "TALK01:1", "en": "Hi{wait:2}", "pt": wide}]},
        "ITMNAM": {"stem": "ITMNAM", "entries": [{"key": "ITMNAM:1", "en": "Hi", "pt": " ".join(["abcd"] * 9)}]},
    }
    out, changes = F.apply_autofit(tpls, decomp, "eu")
    assert list(changes) == ["TALK01:1"] and changes["TALK01:1"][2] == "rewrapped"
    assert out["TALK01"]["entries"][0]["pt"] != wide and "\n" in out["TALK01"]["entries"][0]["pt"]
    assert out["ITMNAM"] is tpls["ITMNAM"]  # a menu window is not a dialogue box
    assert tpls["TALK01"]["entries"][0]["pt"] == wide  # the input stays as the translator wrote it
    stats, _ = F.measure_templates(out, decomp, "eu")
    assert stats.bad_strings[F.TALK] == 0 and stats.strings[F.TALK] == 1


def test_a_name_or_item_word_is_flagged_not_measured():
    assert F.has_insert(enc("Take {buf:1} now"))
    assert F.has_insert(enc("Hi {name:1}!"))
    assert not F.has_insert(enc("Take it now"))


# --------------------------------------------------------------------------
# the builder, the verifier and the checker use it

def _rt_and_templates(tmp_path):
    import extract_text as X
    from test_l10n_extract import flat_metrics, make_runtime

    rt = make_runtime(tmp_path / "rt")
    tpls = X.build_template("eu", rt, {1: flat_metrics(), 2: flat_metrics(), 3: flat_metrics()})
    return rt, tpls


WIDE = " ".join(["abcd"] * 9) + "{wait:2}{pg}Go"  # 9 words of 28 px: one line of 324 px, then a short page


def test_build_rewraps_what_fits_and_verify_measures_the_pack(tmp_path):
    import build_pack as B

    decomp = write_fake_decomp(tmp_path / "decomp")
    rt, tpls = _rt_and_templates(tmp_path)
    tpls["DMG200"]["entries"][1]["pt"] = WIDE
    changes: dict = {}
    out = tmp_path / "pack"
    B.build_pack(tpls, rt, "eu", "ptbr", out, autofit=decomp, fit_changes=changes)
    assert changes["DMG200:0:1"][2] == "rewrapped"
    assert tpls["DMG200"]["entries"][1]["pt"] == WIDE  # the translator's file is untouched
    assert B.verify_pack(out, rt, "eu", decomp=decomp) == []
    # the same string without the rewrap decodes fine and is still a line outside its box: verify says so
    bad = tmp_path / "bad"
    B.build_pack(tpls, rt, "eu", "ptbr", bad)
    assert B.verify_pack(bad, rt, "eu") == []  # the old check: "verified"
    problems = B.verify_pack(bad, rt, "eu", decomp=decomp)
    assert problems and problems[0].startswith("DMG200:0:1: a line is 324 px, the talk box holds 148 px")


def test_a_synthetic_pack_is_not_measured(tmp_path):
    import build_pack as B

    decomp = write_fake_decomp(tmp_path / "decomp")
    rt, _ = _rt_and_templates(tmp_path)
    tp = B.synthetic_templates("stress", "eu", rt, decomp)
    out = tmp_path / "stress"
    B.build_pack(tp, rt, "eu", "ptbr", out, "stress")
    assert B.verify_pack(out, rt, "eu", decomp=decomp) == []  # wide on purpose


def test_check_errors_on_what_cannot_be_fitted_and_warns_on_what_will_be_rewrapped(tmp_path):
    import json

    import extract_text as X

    decomp = write_fake_decomp(tmp_path / "decomp")
    rt, tpls = _rt_and_templates(tmp_path)
    dmg = tpls["DMG200"]
    dmg["entries"][1]["pt"] = WIDE
    dmg["entries"][2]["pt"] = "{start}Mia{start}" + " ".join(["word"] * 20)  # no way into 3 lines
    # the English of entry 2 is not in this fake file; give the three entries texts and keep the codes equal
    dmg["entries"][1]["en"] = "Hi{wait:2}{pg}Go"
    dmg["entries"][2]["en"] = "{start}Mia{start}Hi"
    folder = tmp_path / "tpl"
    folder.mkdir()
    (folder / "DMG200.json").write_text(json.dumps(dmg))
    rep = X.check_paths([folder], strict=False, decomp=decomp, max_width=None, glossary=None, against=None)
    assert any("DMG200:0:1" in w and "rewraps it" in w for w in rep.warnings)
    assert any("DMG200:1:0" in e and "cannot fix it" in e for e in rep.errors)
    assert rep.counts["rewrap_talk"] == 1 and rep.counts["unfit_talk"] == 1


# --------------------------------------------------------------------------
# characters the name bar's font (style 2) has no cell for

def fake_decomp_with_absent(tmp_path: Path) -> Path:
    """Style 2 has no cell (type 255) for icon 4 (the comma) and icon 7 (the colon); everything else is drawn."""
    root = tmp_path / "decomp"
    src = root / "src" / "engine" / "text"
    src.mkdir(parents=True)
    parts = []
    for size in (1, 2, 3):
        for name, n in (("GLYPHS", 230), ("ICONS", 114)):
            cells = []
            for i in range(n):
                kind = 255 if (size == 2 and name == "ICONS" and i + 1 in (4, 7)) else 1
                cells.append("{ %d, 0, 0, 48, 232, 8, 12, 1, 0, 6, 12 }" % kind)
            parts.append("Glyph FONT_%s_%d[%d] = {\n#if VERSION_US\n%s,\n#elif VERSION_EU\n%s,\n#endif\n};\n"
                         % (name, size, n, ", ".join(cells), ", ".join(cells)))
    (src / "text_tools.c").write_text("".join(parts))
    return root


def test_the_name_bar_font_lacks_the_comma(tmp_path):
    absent = T.load_absent(fake_decomp_with_absent(tmp_path), "eu")
    assert absent[2][0] == {4, 7} and absent[1][0] == set() and absent[2][1] == set()
    miss = lambda t: F.speaker_missing_chars(enc(t), absent, EU)  # noqa: E731
    assert miss("{start}Aiko, Lider{start}Hi") == ","  # the engine draws it as a ring
    assert miss("{start}Aiko - Lider{start}Hi") == ""
    assert miss("{start}Junior:{start}Hi") == "" and miss("{start}Junior: {start}Hi") == ""  # the English has it, it draws nothing
    assert miss("{start}A: B{start}Hi") == ":"  # not at the end
    assert miss("Hi, there") == ""  # no speaker: the body is style 1, which has a comma


def test_check_and_verify_flag_a_comma_in_a_speaker_name(tmp_path):
    import json

    import build_pack as B
    import extract_text as X

    decomp = fake_decomp_with_absent(tmp_path)
    rt, tpls = _rt_and_templates(tmp_path)
    dmg = tpls["DMG200"]
    dmg["entries"][1]["en"] = "{start}Mia{start}Hi{wait:2}{pg}Go"
    dmg["entries"][1]["pt"] = "{start}Mia, Lider{start}Oi{wait:2}{pg}Va"
    folder = tmp_path / "tpl"
    folder.mkdir()
    (folder / "DMG200.json").write_text(json.dumps(dmg))
    rep = X.check_paths([folder], strict=False, decomp=decomp, max_width=None, glossary=None, against=None)
    assert any("DMG200:0:1" in e and "cannot draw" in e for e in rep.errors) and rep.counts["name_glyph"] == 1
    stats, _ = F.measure_templates({"DMG200": dmg}, decomp, "eu")
    assert [k for k, _ in stats.name_chars] == ["DMG200:0:1"]
