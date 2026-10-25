"""P6.20 PR 2: page 2 of the Cheats screen (Money, Warp, Item id, Item count,
Give item) and its three hooks (port/options/cheats.c). Uses the harness of
test_cheats.py. docs/PORT_OPTIONS.md "Cheats"."""

from __future__ import annotations

from test_cheats import CFG, cheats_bin, dirs, run  # noqa: F401  (fixtures)

ON = "version=1\ncheat_enabled=1\nend=1\n"


def test_page2_rows_are_inert_while_the_master_is_off(cheats_bin, dirs):
    out, err = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                   "row", "6", "press", "row", "7", "right", "row", "8", "l1",
                   "row", "9", "press", "show2", "money", "100", "9999999",
                   "item", "warp", "sounds")
    assert out[0] == "money=0 warp=0 id=43 count=10 give=0 cursor=9"
    assert out[1:4] == ["money=0 out=100", "item=0 id=0 count=0", "warp=0 mode=0x0"]
    # Nine Down presses moved the cursor; nothing was selected.
    assert out[4] == "cursor=9 select=1 cancel=0"  # 1 = the Square press
    assert not (dirs[0] / CFG).exists()


def test_money_modes_and_the_hook(cheats_bin, dirs):
    (dirs[0] / CFG).write_text(ON)
    out, err = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                   "show2", "money", "100", "9999999", "money", "100",
                   "9999999", "show2", "marked")
    assert out[0].startswith("money=1 ")
    assert out[1] == "money=1 out=10100"
    assert out[2] == "money=0 out=100"          # used up
    assert out[3].startswith("money=0 ")
    assert out[4] == "marked=1"
    assert "dw3-cheats: money +10000: 100 -> 10100 (used up)" in err
    # +100000 is clamped to the maximum, MAX sets it, and a press wraps to OFF.
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                 "press", "show2", "money", "9950000", "9999999")
    assert out[0].startswith("money=2 ") and out[1] == "money=1 out=9999999"
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                 "press", "press", "show2", "money", "5", "9999999")
    assert out[0].startswith("money=3 ") and out[1] == "money=1 out=9999999"
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                 "press", "press", "press", "show2", "money", "5", "9999999")
    assert out[0].startswith("money=0 ") and out[1] == "money=0 out=5"


def test_warp_cycles_the_areas_and_is_used_up(cheats_bin, dirs):
    (dirs[0] / CFG).write_text(ON)
    out, err = run(cheats_bin, dirs, "options", "square", "row", "6", "press",
                   "press", "show2", "warp", "warp", "marked")
    assert out[0].startswith("money=0 warp=2 ")
    assert out[1] == "warp=1 mode=0x206"          # the Digimon Lab
    assert out[2] == "warp=0 mode=0x0"
    assert out[3] == "marked=1"
    assert "dw3-cheats: warp to mode 0x206 (used up)" in err
    # Left from OFF wraps to the last area (Genbu City); 13 areas, then OFF.
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "6", "left",
                 "show2", "warp")
    assert out[0].startswith("money=0 warp=13 ") and out[1] == "warp=1 mode=0x26f"
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "6",
                 *(["press"] * 14), "show2")
    assert out[0].startswith("money=0 warp=0 ")


def test_item_pick_and_give(cheats_bin, dirs):
    (dirs[0] / CFG).write_text(ON)
    out, err = run(cheats_bin, dirs, "options", "square", "row", "7", "right",
                   "right", "r1", "left", "row", "8", "r1", "r1", "left",
                   "show2", "row", "9", "press", "show2", "item", "item",
                   "marked")
    # 43 + 1 + 1 + 10 - 1 = 54; 10 + 10 + 10 - 1 = 29
    assert out[0] == "money=0 warp=0 id=54 count=29 give=0 cursor=8"
    assert out[1] == "money=0 warp=0 id=54 count=29 give=1 cursor=9"
    assert out[2] == "item=1 id=54 count=29"
    assert out[3] == "item=0 id=0 count=0"
    assert out[4] == "marked=1"
    assert "dw3-cheats: give item: id 54 x29 (used up)" in err


def test_item_pick_is_clamped(cheats_bin, dirs):
    # The pick stays inside 43..359 (usable items, weapons, armour and
    # accessories, as Fill bag): no key item 1..42, 360, 395..402 and no
    # card pack 361..394. The count stays inside 1..99.
    (dirs[0] / CFG).write_text(ON)
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "7", "l1", "l1",
                 "left", "row", "8", "l1", "left", "show2", "row", "7",
                 *(["r1"] * 40), "row", "8", *(["r1"] * 12), "show2")
    assert out[0].endswith("id=43 count=1 give=0 cursor=8")
    assert out[1].endswith("id=359 count=99 give=0 cursor=8")


def test_master_off_disarms_page2(cheats_bin, dirs):
    (dirs[0] / CFG).write_text(ON)
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "5", "press",
                 "row", "6", "press", "row", "9", "press", "show2", "row", "0",
                 "press", "show2", "money", "5", "99", "item", "warp")
    assert out[0] == "money=1 warp=1 id=43 count=10 give=1 cursor=9"
    assert out[1] == "money=0 warp=0 id=43 count=10 give=0 cursor=0"
    assert out[2:] == ["money=0 out=5", "item=0 id=0 count=0", "warp=0 mode=0x0"]


def test_arm_variable_page2(cheats_bin, dirs):
    (dirs[0] / CFG).write_text(ON)
    env = {"DW3_CHEATS_ARM": "money_100k,warp=0x233,item=100:7"}
    out, err = run(cheats_bin, dirs, "show2", "money", "0", "9999999", "warp",
                   "item", "marked", env=env)
    assert out[0] == "money=2 warp=8 id=100 count=7 give=1 cursor=0"
    assert out[1:] == ["money=1 out=100000", "warp=1 mode=0x233",
                       "item=1 id=100 count=7", "marked=1"]
    # A key item or an unknown area is not armed.
    out, _ = run(cheats_bin, dirs, "show2",
                 env={"DW3_CHEATS_ARM": "item=5:7,warp=0x999"})
    assert out[0] == "money=0 warp=0 id=43 count=10 give=0 cursor=0"
    # The master switch is needed.
    (dirs[0] / CFG).write_text("version=1\nend=1\n")
    out, _ = run(cheats_bin, dirs, "money", "0", "99", "item", "warp", env=env)
    assert out == ["money=0 out=0", "item=0 id=0 count=0", "warp=0 mode=0x0"]


def test_page_one_is_unchanged(cheats_bin, dirs):
    # Left / Right / L1 / R1 do nothing on page 1, and Down from the last row
    # of page 1 goes to page 2 (row 5).
    (dirs[0] / CFG).write_text(ON)
    out, _ = run(cheats_bin, dirs, "options", "square", "row", "4", "left",
                 "right", "l1", "r1", "show", "row", "5", "show2")
    assert out[0] == "master=1 fill=0 win=0 battle=0 saves=0 open=1 cursor=4 options=1"
    assert out[1].endswith("cursor=5")
