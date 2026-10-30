"""P6.25 slice 2: the `.dw3s` container and the shared per-profile store.

Synthetic only: every card is generated at test time (helpers of
test_save_convert.py) and lives in pytest temp directories; no game or save
bytes are committed. Pure functions (container, canonical <-> native, first-fit
import) run through ctypes on port/memcard/save_store.c; the host behaviour
(opt-in, migration, two-way flow, backups, refusals, crash recovery) runs the C
driver tools/tests/save_store_main.c over port/memcard/memcard_host.c.
"""

from __future__ import annotations

import ctypes
import hashlib
import os
import subprocess
from pathlib import Path

import pytest

from test_save_convert import (DATA_OFF, IMAGE, INFO_OFF, INFO_SIZE, PROGRESS_OFF,
                               REGIONS, SIZE, STRIDE, SUMMARY, VERSION, gcc, make_image,
                               make_slot, xor)

ROOT = Path(__file__).resolve().parents[2]
MEMCARD = ROOT / "port" / "memcard"
CONVERT_C = MEMCARD / "save_convert.c"
STORE_C = MEMCARD / "save_store.c"
HOST_C = MEMCARD / "memcard_host.c"
DRIVER_C = ROOT / "tools" / "tests" / "save_store_main.c"

FILE_SIZE = 0x40 + IMAGE
US, EU = REGIONS["us"], REGIONS["eu"]
TAIL = 0x26BA  # US flags40 bytes 0x1A-0x1B


class Meta(ctypes.Structure):
    _fields_ = [("origin_region", ctypes.c_uint8), ("origin_lang", ctypes.c_uint8),
                ("flags", ctypes.c_uint8), ("us_tail", ctypes.c_uint8 * 2)]


class Card(ctypes.Structure):
    _fields_ = [("image", ctypes.c_uint8 * IMAGE), ("last_region", ctypes.c_uint8),
                ("last_lang", ctypes.c_uint8), ("meta", Meta * 3)]


class WriteRep(ctypes.Structure):
    _fields_ = [("changed", ctypes.c_uint), ("header_changed", ctypes.c_int),
                ("slot_result", ctypes.c_int * 3), ("warn", ctypes.c_uint * 3),
                ("reason", ctypes.c_char * 96)]


class ConvRep(ctypes.Structure):
    _fields_ = [("result", ctypes.c_int * 3), ("warn", ctypes.c_uint * 3),
                ("src_region", ctypes.c_int * 3), ("usable", ctypes.c_int),
                ("converted", ctypes.c_int)]


class ImpSrc(ctypes.Structure):
    _fields_ = [("image", ctypes.c_char_p)]


class ImpRep(ctypes.Structure):
    _fields_ = [("outcome", (ctypes.c_int * 3) * 4), ("target", (ctypes.c_int * 3) * 4),
                ("src_region", (ctypes.c_int * 3) * 4), ("imported", ctypes.c_int),
                ("duplicates", ctypes.c_int), ("invalid", ctypes.c_int),
                ("leftover", ctypes.c_int), ("card_used", ctypes.c_int * 2)]


@pytest.fixture(scope="module")
def lib(tmp_path_factory):
    so = tmp_path_factory.mktemp("savestore_lib") / "libsave_store.so"
    gcc(so, STORE_C, CONVERT_C, shared=True)
    L = ctypes.CDLL(str(so))
    L.save_crc32.restype = ctypes.c_uint32
    L.save_crc32.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    L.save_store_parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(Card)]
    L.save_store_serialize.argtypes = [ctypes.POINTER(Card), ctypes.c_char_p]
    L.save_store_to_native.argtypes = [ctypes.POINTER(Card), ctypes.c_int, ctypes.c_char_p,
                                       ctypes.POINTER(ConvRep)]
    L.save_store_from_native.argtypes = [ctypes.POINTER(Card), ctypes.c_int, ctypes.c_char_p,
                                         ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
                                         ctypes.POINTER(WriteRep)]
    L.save_store_import.argtypes = [ctypes.POINTER(Card), ctypes.POINTER(ImpSrc), ctypes.c_int,
                                    ctypes.POINTER(ImpRep)]
    return L


@pytest.fixture(scope="module")
def driver(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("savestore_drv") / "save_store_main"
    gcc(out, DRIVER_C, HOST_C, CONVERT_C, STORE_C)
    return out


# ----------------------------------------------------------------- helpers

def card_from_image(lib, image: bytes, last: int = 0, meta=()) -> Card:
    c = Card()
    ctypes.memmove(c.image, image, IMAGE)
    c.last_region = last
    c.last_lang = 0xFF
    for i, m in enumerate(meta):
        c.meta[i].origin_region, c.meta[i].origin_lang, c.meta[i].flags = m[:3]
    return c


def serialize(lib, c: Card) -> bytes:
    buf = ctypes.create_string_buffer(FILE_SIZE)
    lib.save_store_serialize(ctypes.byref(c), buf)
    return buf.raw


def parse(lib, data: bytes):
    c = Card()
    return lib.save_store_parse(data, len(data), ctypes.byref(c)), c


def to_native(lib, c: Card, region: str):
    out = ctypes.create_string_buffer(IMAGE)
    rep = ConvRep()
    rc = lib.save_store_to_native(ctypes.byref(c), REGIONS[region], out, ctypes.byref(rep))
    return rc, out.raw, rep


def from_native(lib, c: Card, region: str, native: bytes, base: bytes | None, lang=0xFF,
                final=1):
    rep = WriteRep()
    rc = lib.save_store_from_native(ctypes.byref(c), REGIONS[region], native, base, lang, final,
                                    ctypes.byref(rep))
    return rc, rep


def canonical_image(lib, region: str, slots, seed=1) -> bytes:
    """A canonical (EU layout) card made from a `region` image with these slots."""
    src = make_image(region, slots, seed)
    c = Card()
    imp = ImpRep()
    srcs = (ImpSrc * 4)(ImpSrc(src), ImpSrc(None), ImpSrc(None), ImpSrc(None))
    cards = (Card * 2)()
    lib.save_store_import(cards, srcs, 4, ctypes.byref(imp))
    del c
    return bytes(cards[0].image)


def slot_of(img: bytes, s: int) -> bytes:
    return img[DATA_OFF + s * STRIDE:DATA_OFF + (s + 1) * STRIDE]


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def tree_state(d: Path):
    return {p.name: (sha(p), p.stat().st_mtime_ns) for p in sorted(d.iterdir()) if p.is_file()}


# ------------------------------------------------------------- container

def test_crc32_check_value(lib):
    assert lib.save_crc32(b"123456789", 9) == 0xCBF43926


def test_container_layout_matches_the_design(lib):
    img = make_image("eu", [make_slot("eu", 1), None, None])
    c = card_from_image(lib, img, last=EU, meta=[(EU, 3, 0), (0, 0xFF, 0), (0, 0xFF, 0)])
    c.meta[0].flags = 0x03
    c.meta[0].us_tail[0], c.meta[0].us_tail[1] = 0x12, 0x34
    c.last_lang = 3
    f = serialize(lib, c)
    assert len(f) == FILE_SIZE
    assert f[0:4] == b"DW3S"
    assert int.from_bytes(f[4:6], "little") == 1          # format version
    assert int.from_bytes(f[6:8], "little") == 0x40       # header size
    assert int.from_bytes(f[8:12], "little") == 0x8000    # payload size
    assert int.from_bytes(f[12:16], "little") == lib.save_crc32(f[0x40:], IMAGE)
    assert f[0x10] == EU and f[0x11] == 3
    assert f[0x14:0x19] == bytes([EU, 3, 0x03, 0x12, 0x34])  # slot 0 metadata
    assert f[0x40:] == img
    st, back = parse(lib, f)
    assert st == 0
    assert bytes(back.image) == img and back.last_region == EU and back.last_lang == 3
    assert (back.meta[0].origin_region, back.meta[0].origin_lang, back.meta[0].flags) == (EU, 3, 3)
    assert list(back.meta[0].us_tail) == [0x12, 0x34]


def test_parse_refuses_what_it_cannot_trust(lib):
    img = make_image("eu", [make_slot("eu", 1), None, None])
    good = serialize(lib, card_from_image(lib, img, last=EU))
    assert parse(lib, good)[0] == 0
    # larger header and trailing bytes are ignored (forward rule)
    wide = bytearray(good[:0x40]) + bytes(8) + good[0x40:] + b"tail"
    wide[6:8] = (0x48).to_bytes(2, "little")
    assert parse(lib, bytes(wide))[0] == 0
    assert parse(lib, b"")[0] == 1 and parse(lib, good[:0x20])[0] == 1
    assert parse(lib, good[:-1])[0] == 1                          # truncated payload
    assert parse(lib, b"XW3S" + good[4:])[0] == 2                 # magic
    zero_fmt = bytearray(good)
    zero_fmt[4:6] = bytes(2)
    assert parse(lib, bytes(zero_fmt))[0] == 3                    # format 0
    newer = bytearray(good)
    newer[4:6] = (2).to_bytes(2, "little")
    st, c = parse(lib, bytes(newer))
    assert st == 4 and bytes(c.image) == bytes(IMAGE)             # newer: not read, zeroed
    big = bytearray(good)
    big[8:12] = (0x9000).to_bytes(4, "little")
    assert parse(lib, bytes(big))[0] == 3                         # payload size
    flip = bytearray(good)
    flip[0x40 + DATA_OFF + 77] ^= 1
    assert parse(lib, bytes(flip))[0] == 5                        # crc
    bad_info = bytearray(good)
    bad_info[0x40 + INFO_OFF + 4] ^= 0xFF                         # magic broken, crc redone
    bad_info[12:16] = lib.save_crc32(bytes(bad_info[0x40:]), IMAGE).to_bytes(4, "little")
    assert parse(lib, bytes(bad_info))[0] == 6


# ------------------------------------------------- canonical <-> native

def eu_card(lib, slots, seed=1, meta=()):
    return card_from_image(lib, make_image("eu", slots, seed), last=EU, meta=meta)


def test_eu_view_is_the_canonical_image(lib):
    s = [make_slot("eu", 1), make_slot("eu", 2), None]
    c = eu_card(lib, s)
    rc, nat, rep = to_native(lib, c, "eu")
    assert rc == 0 and list(rep.result) == [1, 1, 0]
    for i in (0, 1):
        assert slot_of(nat, i) == slot_of(bytes(c.image), i)
    assert nat[:INFO_OFF] == bytes(c.image[:INFO_OFF])


def test_us_round_trip_native_canonical_native_is_identity(lib):
    us = [make_slot("us", 11), make_slot("us", 12, progress=0x20), None]
    img = make_image("us", us, seed=5)
    c = Card()
    rc, rep = from_native(lib, c, "us", img, None)
    assert rc == 0 and rep.changed == 0b011 and rep.header_changed == 1
    assert (c.meta[0].origin_region, c.meta[1].origin_region, c.meta[2].origin_region) == (US, US, 0)
    st, parsed = parse(lib, serialize(lib, c))
    assert st == 0
    rc, nat, nrep = to_native(lib, parsed, "us")
    assert rc == 0 and list(nrep.result) == [2, 2, 0]
    for i in (0, 1):  # identical over GAME_SAVE_SIZE, so the checksum matches too
        assert slot_of(nat, i)[:SIZE["us"]] == slot_of(img, i)[:SIZE["us"]]
        assert slot_of(nat, i)[2] == 3 and slot_of(nat, i)[0] == xor(slot_of(nat, i)[4:SIZE["us"]])
    assert nat[:INFO_OFF] == img[:INFO_OFF]
    # info: same summaries, US version byte, exact checksum
    assert nat[INFO_OFF + 4:INFO_OFF + INFO_SIZE] == img[INFO_OFF + 4:INFO_OFF + INFO_SIZE]
    assert nat[INFO_OFF + 2] == 3
    assert nat[INFO_OFF] == xor(nat[INFO_OFF + 4:INFO_OFF + INFO_SIZE])


def test_us_flags40_tail_survives_us_to_eu_to_us(lib):
    s = make_slot("us", 21)
    s[TAIL], s[TAIL + 1] = 0xA5, 0x5A   # stray bits in the two bytes EU has no place for
    s[0] = xor(bytes(s[4:SIZE["us"]]))
    img = make_image("us", [s, None, None])
    c = Card()
    assert from_native(lib, c, "us", img, None)[0] == 0
    assert c.meta[0].flags & 2 and list(c.meta[0].us_tail) == [0xA5, 0x5A]
    # the EU view has no such bytes, but the US view gets them back, checksum redone
    _, eu_nat, _ = to_native(lib, c, "eu")
    assert xor(slot_of(eu_nat, 0)[4:SIZE["eu"]]) == slot_of(eu_nat, 0)[0]
    _, us_nat, _ = to_native(lib, c, "us")
    assert slot_of(us_nat, 0)[:SIZE["us"]] == bytes(s[:SIZE["us"]])


def test_eu_extra_chapter_slot_is_shown_to_us_as_error_8_and_stays_untouched(lib):
    extra = make_slot("eu", 31, progress=0x2D)
    base = make_slot("eu", 32, progress=6)
    c = eu_card(lib, [extra, base, None])
    c.meta[0].flags = 1
    before = bytes(c.image)
    rc, nat, rep = to_native(lib, c, "us")
    assert rc == 0 and list(rep.result) == [3, 2, 0]
    assert slot_of(nat, 0) == slot_of(before, 0)          # EU bytes, version 4: US game says error 8
    assert slot_of(nat, 0)[2] == 4
    # the US game saves over slot 1 only: slot 0 stays byte for byte, and keeps its metadata
    us_slot = make_slot("us", 33, progress=7)
    newimg = bytearray(nat)
    newimg[DATA_OFF + STRIDE:DATA_OFF + 2 * STRIDE] = us_slot
    info = bytearray(newimg[INFO_OFF:DATA_OFF])
    info[2] = 3
    info[0] = xor(bytes(info[4:INFO_SIZE]))
    newimg[INFO_OFF:DATA_OFF] = info
    rc, wrep = from_native(lib, c, "us", bytes(newimg), nat)
    assert rc == 0 and wrep.changed == 0b010
    assert slot_of(bytes(c.image), 0) == slot_of(before, 0)
    assert c.meta[0].flags == 1 and c.meta[0].origin_region == 0
    assert c.meta[1].origin_region == US and not c.meta[1].flags & 1


def test_only_slots_the_game_wrote_are_converted(lib):
    c = eu_card(lib, [make_slot("eu", 1), make_slot("eu", 2), make_slot("eu", 3)])
    keep = bytes(c.image)
    _, nat, _ = to_native(lib, c, "us")
    # nothing written -> nothing changes (not even a rewrite of the same bytes)
    rc, rep = from_native(lib, c, "us", nat, nat)
    assert rc == 0 and rep.changed == 0 and rep.header_changed == 0
    assert bytes(c.image) == keep
    # write slot 2 only
    new = bytearray(nat)
    new[DATA_OFF + 2 * STRIDE:DATA_OFF + 3 * STRIDE] = make_slot("us", 9)
    rc, rep = from_native(lib, c, "us", bytes(new), nat, lang=0xFF)
    assert rc == 0 and rep.changed == 0b100
    assert slot_of(bytes(c.image), 0) == slot_of(keep, 0)
    assert slot_of(bytes(c.image), 1) == slot_of(keep, 1)
    assert slot_of(bytes(c.image), 2)[2] == 4
    assert c.last_region == US


def test_origin_language_is_recorded_for_an_eu_write(lib):
    c = Card()
    img = make_image("eu", [make_slot("eu", 5), None, None])
    assert from_native(lib, c, "eu", img, None, lang=4)[0] == 0
    assert (c.meta[0].origin_region, c.meta[0].origin_lang) == (EU, 4)
    assert (c.last_region, c.last_lang) == (EU, 4)
    c2 = Card()
    assert from_native(lib, c2, "us", make_image("us", [make_slot("us", 5), None, None]),
                       None, lang=4)[0] == 0
    assert c2.meta[0].origin_lang == 0xFF  # US has no language


def test_a_save_past_the_base_game_is_flagged_as_needing_the_extra_chapter(lib):
    c = Card()
    img = make_image("eu", [make_slot("eu", 5, progress=0x2D), make_slot("eu", 6, progress=0x2C),
                            None])
    assert from_native(lib, c, "eu", img, None)[0] == 0
    assert c.meta[0].flags & 1 and not c.meta[1].flags & 1


def test_from_native_waits_for_a_settled_image_and_leaves_the_card_alone(lib):
    c = eu_card(lib, [make_slot("eu", 1), None, None])
    keep = bytes(c.image)
    _, nat, _ = to_native(lib, c, "eu")
    # a slot the game wrote whose checksum is wrong: not settled (1), the card is untouched
    bad = bytearray(nat)
    bad[DATA_OFF + 100] ^= 0x01
    rc, rep = from_native(lib, c, "eu", bytes(bad), nat)
    assert rc == 1 and b"slot 0" in rep.reason and bytes(c.image) == keep
    # an info section that is not a DW3 card (a save in progress: the header went first)
    bad = bytearray(nat)
    bad[INFO_OFF + 5] ^= 0xFF
    rc, rep = from_native(lib, c, "eu", bytes(bad), nat)
    assert rc == 1 and b"info" in rep.reason and bytes(c.image) == keep
    # a brand new card: the info section is named but the slot is still zeros
    new = bytearray(IMAGE)
    new[INFO_OFF:DATA_OFF] = nat[INFO_OFF:DATA_OFF]
    assert from_native(lib, Card(), "eu", bytes(new), None)[0] == 1
    # an unknown version byte in the slot
    junk = bytearray(nat)
    junk[DATA_OFF + 2] = 9
    assert from_native(lib, eu_card(lib, [make_slot("eu", 1), None, None]), "us", bytes(junk), nat)[0] == 1
    # a US-layout slot under an EU view is a valid save of the other region: converted
    us = bytearray(nat)
    us[DATA_OFF:DATA_OFF + STRIDE] = make_slot("us", 4)
    assert from_native(lib, c, "eu", bytes(us), nat)[0] == 0


def test_an_info_only_change_waits_for_the_slot_data_until_the_end(lib):
    c = eu_card(lib, [make_slot("eu", 1), None, None])
    _, nat, _ = to_native(lib, c, "eu")
    keep = bytes(c.image)
    info_only = bytearray(nat)
    info_only[INFO_OFF + 8:INFO_OFF + 8 + 8] = b"NewName!"
    info_only[INFO_OFF] = xor(bytes(info_only[INFO_OFF + 4:INFO_OFF + INFO_SIZE]))
    rc, rep = from_native(lib, c, "eu", bytes(info_only), nat, final=0)
    assert rc == 1 and b"waiting for the slot data" in rep.reason and bytes(c.image) == keep
    rc, rep = from_native(lib, c, "eu", bytes(info_only), nat, final=1)   # the process is ending
    assert rc == 0 and rep.header_changed == 1 and rep.changed == 0
    assert bytes(c.image)[INFO_OFF + 8:INFO_OFF + 16] == b"NewName!"


# --------------------------------------------------------------- import

def run_import(lib, images):
    srcs = (ImpSrc * 4)()
    for i in range(4):
        srcs[i] = ImpSrc(images[i] if i < len(images) else None)
    cards = (Card * 2)()
    rep = ImpRep()
    n = lib.save_store_import(cards, srcs, 4, ctypes.byref(rep))
    return n, cards, rep


def test_import_first_fit_of_up_to_six_slots(lib):
    for total in (0, 1, 6, 7):
        us = make_image("us", [make_slot("us", 100 + i) if i < min(total, 3) else None
                               for i in range(3)], seed=1)
        us_b = make_image("us", [make_slot("us", 200 + i) if i < max(0, min(total - 3, 3)) else None
                                 for i in range(3)], seed=2)
        eu_a = make_image("eu", [make_slot("eu", 300) if total >= 7 else None, None, None], seed=3)
        n, cards, rep = run_import(lib, [us, None, us_b, eu_a])
        assert n == min(total, 6), total
        assert rep.leftover == (1 if total == 7 else 0)
        assert rep.card_used[0] == (1 if total > 0 else 0)
        assert rep.card_used[1] == (1 if total > 3 else 0)
        for c in range(2):
            if rep.card_used[c]:
                st, _ = parse(lib, serialize(lib, cards[c]))
                assert st == 0  # a card the import made is a valid container
                info = bytes(cards[c].image)[INFO_OFF:DATA_OFF]
                assert info[2] == 4 and info[0] == xor(info[4:INFO_SIZE])


def test_import_order_dedupe_and_refusals(lib, tmp_path):
    a = make_slot("us", 1)
    b = make_slot("eu", 2)
    bad = make_slot("us", 3)
    bad[0] ^= 0xFF  # wrong checksum
    ext = make_slot("eu", 4, progress=0x2D)
    src_us = make_image("us", [a, bad, None], seed=1)
    # slice 1 left a converted copy of `a` in the EU folder, next to two EU saves
    copy = slice1_copy(make_image("us", [a, None, None], seed=1), tmp_path)
    eu = bytearray(make_image("eu", [None, b, ext], seed=9))
    eu[DATA_OFF:DATA_OFF + STRIDE] = slot_of(copy, 0)
    eu[INFO_OFF + 8:INFO_OFF + 8 + SUMMARY] = copy[INFO_OFF + 8:INFO_OFF + 8 + SUMMARY]
    eu[INFO_OFF] = xor(bytes(eu[INFO_OFF + 4:INFO_OFF + INFO_SIZE]))
    n, cards, rep = run_import(lib, [src_us, bytes(eu), None, None])
    assert n == 3 and rep.duplicates == 1 and rep.invalid == 1
    assert list(rep.outcome[0]) == [1, 3, 0]
    assert list(rep.outcome[1]) == [2, 1, 1]
    assert rep.target[0][0] == 0 and rep.target[1][1] == 1 and rep.target[1][2] == 2
    assert rep.src_region[0][0] == US and rep.src_region[1][1] == EU
    assert cards[0].meta[0].origin_region == US and cards[0].meta[1].origin_region == EU
    assert cards[0].meta[2].flags & 1       # the extra-chapter save is flagged
    assert slot_of(bytes(cards[0].image), 1)[:SIZE["eu"]] == bytes(b[:SIZE["eu"]])
    # the summary of each save travelled with it
    nm = bytes(cards[0].image)[INFO_OFF + 8 + SUMMARY:INFO_OFF + 8 + SUMMARY + 8]
    assert nm == b"Tester01"
    # the sources are only read
    h = hashlib.sha256(src_us).hexdigest()
    run_import(lib, [src_us])
    assert hashlib.sha256(src_us).hexdigest() == h


def test_import_refuses_a_source_that_is_not_a_card(lib):
    noise = os.urandom(IMAGE)
    n, cards, rep = run_import(lib, [noise, bytes(IMAGE)])
    assert n == 0 and rep.outcome[0][0] == 5 and rep.outcome[1][0] == 5
    assert rep.card_used[0] == 0 and bytes(cards[0].image) == bytes(IMAGE)


# ---------------------------------------------------------------- host

class Env:
    """A profile on disk: <root>/us/memcard, <root>/eu/memcard, <root>/store."""

    def __init__(self, root: Path, driver: Path):
        self.root, self.driver = root, driver
        self.dir = {"us": root / "us" / "memcard", "eu": root / "eu" / "memcard"}
        self.store = root / "store"
        for d in (*self.dir.values(), self.store):
            d.mkdir(parents=True)

    def legacy(self, region: str, n: int, image: bytes) -> Path:
        p = self.dir[region] / f"save{n}.bin"
        p.write_bytes(image)
        return p

    def run(self, region: str, *ops: str, store=True, env=None, cfg=None, store_dir="default"):
        e = dict(os.environ)
        e.pop("DW3_SAVE_STORE", None)
        e.pop("DW3_SAVE_STORE_DIR", None)
        if store is not None:
            e["DW3_SAVE_STORE"] = "1" if store else "0"
        e.update(env or {})
        other = "eu" if region == "us" else "us"
        sd = str(self.store) if store_dir == "default" else (store_dir or "-")
        r = subprocess.run([str(self.driver), region, str(self.dir[region]),
                            str(self.dir[other]), sd, cfg or "-", *ops],
                           capture_output=True, text=True, env=e)
        return r

    def card(self, n=0) -> Path:
        return self.store / f"card{n}.dw3s"

    def native(self, region: str, n: int, tmp: Path) -> bytes:
        out = tmp / f"dump-{region}-{n}.bin"
        r = self.run(region, f"ensure:{n}", f"dump:{n}:{out}")
        assert r.returncode == 0, r.stderr
        return out.read_bytes()


@pytest.fixture
def env(tmp_path, driver):
    return Env(tmp_path / "profile", driver)


def test_option_off_is_slice_1_nothing_is_written_to_the_store(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None]))
    for store in (False, None):
        r = env.run("us", f"load:0:{img}", "shutdown", store=store)
        assert r.returncode == 0 and "store setup=-1" in r.stdout
        assert "dw3-store" not in r.stderr
        assert (env.dir["us"] / "save0.bin").exists()
        assert not list(env.store.iterdir())
        (env.dir["us"] / "save0.bin").unlink()
        (env.dir["us"] / "save0.name").unlink()


def test_option_in_the_cfg_file(env, tmp_path):
    cfg = tmp_path / "cfg"
    cfg.mkdir()
    (cfg / "port_options.cfg").write_text("version=1\nsave_store=1\nend=1\n")
    r = env.run("us", "ensure:0", store=None, cfg=str(cfg))
    assert "store setup=0" in r.stdout and "dw3-store: on dir=" in r.stderr
    (cfg / "port_options.cfg").write_text("version=1\nsave_store=2\nend=1\n")  # invalid: off
    r = env.run("us", "ensure:0", store=None, cfg=str(cfg))
    assert "store setup=-1" in r.stdout and "save_store=2 is not valid" in r.stderr
    (cfg / "port_options.cfg").write_text("save_store=1 \r\n")                 # trailing blanks are fine
    assert "store setup=0" in env.run("us", "ensure:0", store=None, cfg=str(cfg)).stdout
    (cfg / "port_options.cfg").write_text("save_store=1\nsave_store=0\n")       # last wins
    assert "store setup=-1" in env.run("us", "ensure:0", store=None, cfg=str(cfg)).stdout
    (cfg / "port_options.cfg").write_text("save_store=1\n")                    # env beats the file
    assert "store setup=-1" in env.run("us", "ensure:0", store=False, cfg=str(cfg)).stdout


def test_option_on_without_a_profile_folder_stays_off(env):
    r = env.run("us", "ensure:0", store_dir="")
    assert "store setup=-1" in r.stdout and "no profile folder" in r.stderr
    assert "store=0" in r.stdout


def test_migration_copies_and_keeps_the_originals(env, tmp_path):
    us = env.legacy("us", 0, make_image("us", [make_slot("us", 1), make_slot("us", 2), None], 1))
    eu = env.legacy("eu", 0, make_image("eu", [make_slot("eu", 3), None, None], 2))
    marker = env.dir["eu"] / "save0.import"
    marker.write_text("dw3-cross-region-import 1\nold marker\n")
    (env.dir["us"] / "save0.name").write_text("BASLUS-01436DMW3-USA")
    before = {r: tree_state(env.dir[r]) for r in ("us", "eu")}
    r = env.run("eu", "ensure:0", "ensure:1", "shutdown")
    assert r.returncode == 0, r.stderr
    assert "ensure 0 rc=0 has=1 store=1" in r.stdout and "ensure 1 rc=0 has=0 store=1" in r.stdout
    assert before == {r: tree_state(env.dir[r]) for r in ("us", "eu")}   # nothing touched
    assert env.card(0).exists() and not env.card(1).exists()
    assert (env.store / "imported.txt").exists()
    assert "migration done imported=3 duplicate=0 refused=0 left_in_legacy=0" in r.stderr
    assert r.stderr.count(" -> card0 slot ") == 3
    # US A first, then EU A (design order)
    f = env.card(0).read_bytes()
    assert f[0x14] == US and f[0x19] == US and f[0x1E] == EU
    # the second start imports nothing again, and a deleted card does not come back
    r2 = env.run("us", "ensure:0")
    assert "migrate" not in r2.stderr
    saved = env.card(0).read_bytes()
    env.card(0).rename(tmp_path / "moved.dw3s")
    r3 = env.run("us", "ensure:0")
    assert "has=0" in r3.stdout and not env.card(0).exists()
    (tmp_path / "moved.dw3s").rename(env.card(0))
    assert env.card(0).read_bytes() == saved


_TOOL: list[Path] = []


def slice1_copy(us_image: bytes, tmp_path: Path, name: str = "copy") -> bytes:
    """What slice 1 wrote into the EU folder for a US card (save_convert_tool)."""
    if not _TOOL:
        import tempfile
        out = Path(tempfile.mkdtemp(prefix="p625-tool-")) / "tool"
        gcc(out, ROOT / "tools" / "save_convert_tool.c", CONVERT_C)
        _TOOL.append(out)
    src, dst = tmp_path / f"{name}-in.bin", tmp_path / f"{name}-out.bin"
    src.write_bytes(us_image)
    r = subprocess.run([str(_TOOL[0]), "convert", str(src), str(dst), "eu"],
                       capture_output=True)
    assert r.returncode == 0, r.stderr
    return dst.read_bytes()


def test_migration_of_a_slice_1_converted_copy_is_not_a_duplicate_save(env, tmp_path):
    """The EU folder holds the converted copy slice 1 made of the US save."""
    base = make_image("us", [make_slot("us", 1), None, None], 1)
    env.legacy("us", 0, base)
    env.legacy("eu", 0, slice1_copy(base, tmp_path))
    r = env.run("us", "ensure:0")
    assert "duplicate of one already imported" in r.stderr
    assert "migration done imported=1 duplicate=1" in r.stderr


def test_two_way_flow_us_to_eu_and_back(env, tmp_path):
    img = tmp_path / "game.bin"
    us_slot = make_slot("us", 41, progress=6)
    img.write_bytes(make_image("us", [us_slot, None, None], 3))
    # the US build saves: the card appears in the store, saveN.bin is not written
    r = env.run("us", f"load:0:{img}", "shutdown")
    assert r.returncode == 0 and "dw3-store: write card0 by us slots_changed=100" in r.stderr
    assert env.card(0).exists() and not (env.dir["us"] / "save0.bin").exists()
    first = env.card(0).read_bytes()
    # the EU build sees it, converted, and its summary
    eu_img = env.native("eu", 0, tmp_path)
    eu_slot = slot_of(eu_img, 0)
    assert eu_slot[2] == 4 and eu_slot[0] == xor(eu_slot[4:SIZE["eu"]])
    assert eu_slot[4:PROGRESS_OFF + 4] == bytes(us_slot[4:PROGRESS_OFF + 4])  # the common block
    assert eu_slot[0x263C:0x2640] == us_slot[0x263C:0x2640]
    assert eu_img[INFO_OFF + 8:INFO_OFF + 8 + 8] == b"Tester00"
    assert env.card(0).read_bytes() == first   # opening writes nothing
    # the EU build saves a second game in slot 1 of the same card
    eu_new = bytearray(eu_img)
    eu_slot1 = make_slot("eu", 42, progress=9)
    eu_new[DATA_OFF + STRIDE:DATA_OFF + 2 * STRIDE] = eu_slot1
    info = bytearray(eu_new[INFO_OFF:DATA_OFF])
    info[8 + SUMMARY:8 + SUMMARY + 8] = b"EuSave01"
    info[0] = xor(bytes(info[4:INFO_SIZE]))
    eu_new[INFO_OFF:DATA_OFF] = info
    g2 = tmp_path / "eu-game.bin"
    g2.write_bytes(bytes(eu_new))
    r = env.run("eu", "ensure:0", f"load:0:{g2}", "shutdown")
    assert "write card0 by eu slots_changed=010" in r.stderr, r.stderr
    assert "backup card0.dw3s -> card0.dw3s." in r.stderr
    baks = sorted(env.store.glob("card0.dw3s.*.bak"))
    assert len(baks) == 1 and baks[0].read_bytes() == first
    # back in the US build: both saves are there, slot 0 unchanged
    us_img = env.native("us", 0, tmp_path)
    assert slot_of(us_img, 0)[:SIZE["us"]] == bytes(us_slot[:SIZE["us"]])
    s1 = slot_of(us_img, 1)
    assert s1[2] == 3 and s1[0] == xor(s1[4:SIZE["us"]]) and s1[0x263C:0x2640] == eu_slot1[0x263C:0x2640]
    assert us_img[INFO_OFF + 8 + SUMMARY:INFO_OFF + 8 + SUMMARY + 8] == b"EuSave01"


def test_one_backup_per_session_never_deleted(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    img2 = tmp_path / "game2.bin"
    img2.write_bytes(make_image("us", [make_slot("us", 2), None, None], 3))
    img3 = tmp_path / "game3.bin"
    img3.write_bytes(make_image("us", [make_slot("us", 3), None, None], 3))
    r = env.run("us", "ensure:0", f"load:0:{img2}", "flush:0", f"load:0:{img3}", "flush:0", "shutdown")
    assert r.stderr.count("dw3-store: backup card0") == 1       # once per session, not per flush
    assert r.stderr.count("dw3-store: write card0") == 2
    r = env.run("us", "ensure:0", f"load:0:{img}", "flush:0", "shutdown")
    assert len(list(env.store.glob("card0.dw3s.*.bak*"))) >= 2  # a later session adds, never removes


def test_extra_chapter_is_error_8_in_us_and_survives_a_us_save(env, tmp_path):
    extra = make_slot("eu", 51, progress=0x2D)
    env.legacy("eu", 0, make_image("eu", [extra, None, None], 4))
    r = env.run("us", "ensure:0", "shutdown")
    assert "migration done imported=1" in r.stderr
    assert "slot 0 needs the EU extra chapter" in r.stderr
    saved = env.card(0).read_bytes()
    us_img = env.native("us", 0, tmp_path)
    assert slot_of(us_img, 0)[2] == 4          # version 4 in the US build: the game's own error 8
    # the US build saves into slot 1: slot 0 is untouched in the store
    new = bytearray(us_img)
    new[DATA_OFF + STRIDE:DATA_OFF + 2 * STRIDE] = make_slot("us", 52)
    info = bytearray(new[INFO_OFF:DATA_OFF])
    info[8 + SUMMARY:8 + SUMMARY + 8] = b"UsSlot01"
    info[0] = xor(bytes(info[4:INFO_SIZE]))
    new[INFO_OFF:DATA_OFF] = info
    g = tmp_path / "us-game.bin"
    g.write_bytes(bytes(new))
    r = env.run("us", "ensure:0", f"load:0:{g}", "shutdown")
    assert "write card0 by us slots_changed=010" in r.stderr
    after = env.card(0).read_bytes()
    assert slot_of(after[0x40:], 0) == slot_of(saved[0x40:], 0)
    assert after[0x14:0x19] == saved[0x14:0x19]            # its metadata too
    assert len(list(env.store.glob("card0.dw3s.*.bak"))) == 1
    # and the EU build still plays it
    eu_img = env.native("eu", 0, tmp_path)
    assert slot_of(eu_img, 0)[:SIZE["eu"]] == bytes(extra[:SIZE["eu"]])


@pytest.mark.parametrize("damage", ["crc", "truncated", "magic", "newer", "layout"])
def test_a_card_that_cannot_be_trusted_turns_the_store_off_and_is_left_alone(env, tmp_path, damage, lib):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    f = bytearray(env.card(0).read_bytes())
    if damage == "crc":
        f[0x40 + DATA_OFF + 9] ^= 1
    elif damage == "truncated":
        f = f[:FILE_SIZE - 100]
    elif damage == "magic":
        f[0:4] = b"NOPE"
    elif damage == "newer":
        f[4:6] = (7).to_bytes(2, "little")
    else:
        f[0x40 + INFO_OFF + 4] ^= 0xFF
        f[12:16] = lib.save_crc32(bytes(f[0x40:]), IMAGE).to_bytes(4, "little")
    env.card(0).write_bytes(bytes(f))
    h = sha(env.card(0))
    legacy = env.legacy("us", 0, make_image("us", [make_slot("us", 77), None, None], 5))
    r = env.run("us", "ensure:0", f"load:0:{img}", "shutdown")
    assert "card0.dw3s refused" in r.stderr and "file left untouched" in r.stderr
    assert "ensure 0 rc=0 has=1 store=0" in r.stdout          # flat saves serve the port (slice 1)
    assert sha(env.card(0)) == h
    assert not list(env.store.glob("*.bak"))
    assert legacy.exists()


def test_crash_after_the_old_file_was_removed_is_recovered(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    img2 = tmp_path / "game2.bin"
    img2.write_bytes(make_image("us", [make_slot("us", 2), None, None], 3))
    # the 2nd atomic write after arming is the card (the 1st is the backup): die after the
    # old file was removed, before the rename
    r = env.run("us", "ensure:0", "fault:4:2", f"load:0:{img2}", "flush:0", "abort")
    assert r.returncode == 3 and "flush 0 rc=-1" in r.stdout
    assert not env.card(0).exists() and (env.store / "card0.dw3s.tmp").exists()
    r = env.run("us", "ensure:0")
    assert "recovered" in r.stderr and "ensure 0 rc=0 has=1 store=1" in r.stdout
    assert env.card(0).exists() and len(list(env.store.glob("card0.dw3s.*.bak"))) == 1


def test_crash_half_way_keeps_the_old_card(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    h = sha(env.card(0))
    img2 = tmp_path / "game2.bin"
    img2.write_bytes(make_image("us", [make_slot("us", 2), None, None], 3))
    r = env.run("us", "ensure:0", "fault:2:2", f"load:0:{img2}", "flush:0", "abort")
    assert "flush 0 rc=-1 failed=1" in r.stdout
    assert sha(env.card(0)) == h
    r = env.run("us", "ensure:0")
    assert "removed stale" in r.stderr and not (env.store / "card0.dw3s.tmp").exists()
    assert sha(env.card(0)) == h


def test_a_failed_backup_refuses_the_write(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    h = sha(env.card(0))
    img2 = tmp_path / "game2.bin"
    img2.write_bytes(make_image("us", [make_slot("us", 2), None, None], 3))
    os.chmod(env.store, 0o555)
    try:
        if os.access(env.store, os.W_OK):
            pytest.skip("directory permissions are not enforced here (root)")
        r = env.run("us", "ensure:0", f"load:0:{img2}", "flush:0", "shutdown")
    finally:
        os.chmod(env.store, 0o755)
    assert "backup of card0.dw3s FAILED: not writing" in r.stderr
    assert "flush 0 rc=-1 failed=1" in r.stdout
    assert sha(env.card(0)) == h


def test_a_slot_the_game_wrote_badly_is_never_stored(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    h = sha(env.card(0))
    bad = bytearray(make_image("us", [make_slot("us", 2), None, None], 3))
    bad[DATA_OFF + 50] ^= 0xFF  # checksum no longer matches
    g = tmp_path / "bad.bin"
    g.write_bytes(bytes(bad))
    r = env.run("us", "ensure:0", f"load:0:{g}", "flush:0", "shutdown")
    assert "not settled yet" in r.stderr and "flush 0 rc=0 failed=0" in r.stdout   # waits
    assert "REFUSED to write card0" in r.stderr and "slot 0 is not a valid save" in r.stderr  # the end
    assert sha(env.card(0)) == h and not list(env.store.glob("*.bak"))


def test_a_save_in_bursts_writes_the_card_once_it_is_whole(env, tmp_path):
    """The game writes the header, the info section, then the slot, with a flush
    between the bursts: nothing is written before the slot is in."""
    us = make_image("us", [make_slot("us", 7), None, None], 3)
    parts = {"hdr": us[:INFO_OFF], "info": us[INFO_OFF:DATA_OFF], "slot": us[DATA_OFF:DATA_OFF + STRIDE]}
    files = {}
    for k, v in parts.items():
        files[k] = tmp_path / f"{k}.bin"
        files[k].write_bytes(v)
    r = env.run("us", "ensure:0", f"put:0:0:{files['hdr']}", "flush:0",
                f"put:0:{INFO_OFF}:{files['info']}", "flush:0",
                f"put:0:{DATA_OFF}:{files['slot']}", "flush:0", "shutdown")
    assert r.stderr.count("dw3-store: write card0") == 1
    assert "slots_changed=100 header=1" in r.stderr
    assert "REFUSED" not in r.stderr
    # a second save over the same slot: the info burst alone is not written, the slot burst is
    us2 = make_image("us", [make_slot("us", 8), None, None], 3)
    info2, slot2 = tmp_path / "info2.bin", tmp_path / "slot2.bin"
    info2.write_bytes(us2[INFO_OFF:DATA_OFF])
    slot2.write_bytes(us2[DATA_OFF:DATA_OFF + STRIDE])
    r = env.run("us", "ensure:0", f"put:0:{INFO_OFF}:{info2}", "flush:0",
                f"put:0:{DATA_OFF}:{slot2}", "flush:0", "shutdown")
    assert r.stderr.count("dw3-store: write card0") == 1 and "slots_changed=100" in r.stderr
    assert len(list(env.store.glob("card0.dw3s.*.bak"))) == 1


def test_a_new_card_with_no_save_is_not_worth_a_file(env, tmp_path):
    """The game formats the file (header, empty info) before the first save."""
    empty = make_image("us", [None, None, None], 3)
    f = tmp_path / "empty.bin"
    f.write_bytes(empty[:DATA_OFF])
    r = env.run("us", "ensure:0", f"put:0:0:{f}", "flush:0", "shutdown")
    assert r.returncode == 0 and not env.card(0).exists()
    assert "dw3-store: write card0" not in r.stderr and "REFUSED" not in r.stderr


def test_format_never_deletes_the_card(env, tmp_path):
    img = tmp_path / "game.bin"
    img.write_bytes(make_image("us", [make_slot("us", 1), None, None], 3))
    env.run("us", f"load:0:{img}", "shutdown")
    h = sha(env.card(0))
    r = env.run("us", "ensure:0", "format:0", "ensure:0", "shutdown")
    assert "format 0 rc=0 has=0" in r.stdout and "ensure 0 rc=0 has=0 store=1" in r.stdout
    assert "formatted by the game" in r.stderr
    assert sha(env.card(0)) == h                       # still there, still the old content
    # the next save replaces it, after a backup
    r = env.run("us", "ensure:0", f"load:0:{img}", "shutdown")
    assert env.card(0).exists() and "dw3-store: backup card0" in r.stderr
    assert len(list(env.store.glob("card0.dw3s.*.bak"))) == 1


def test_the_store_never_writes_or_deletes_the_flat_saves(env, tmp_path):
    env.legacy("us", 0, make_image("us", [make_slot("us", 1), None, None], 1))
    env.legacy("eu", 1, make_image("eu", [make_slot("eu", 2), None, None], 2))
    (env.dir["us"] / "save0.name").write_text("BASLUS-01436DMW3-USA")
    before = {r: tree_state(env.dir[r]) for r in ("us", "eu")}
    g = tmp_path / "game.bin"
    g.write_bytes(make_image("eu", [make_slot("eu", 9), None, None], 7))
    env.run("eu", "ensure:0", "ensure:1", f"put:0:0:{g}", "flush:0", "format:1", "shutdown")
    env.run("us", "ensure:0", "ensure:1", "shutdown")
    assert before == {r: tree_state(env.dir[r]) for r in ("us", "eu")}


def test_card_b_gets_the_second_set_of_saves(env, tmp_path):
    env.legacy("us", 0, make_image("us", [make_slot("us", i) for i in (1, 2, 3)], 1))
    env.legacy("us", 1, make_image("us", [make_slot("us", 4), None, None], 2))
    env.legacy("eu", 0, make_image("eu", [make_slot("eu", 5), None, None], 3))
    r = env.run("eu", "ensure:0", "ensure:1", "shutdown")
    assert "migration done imported=5" in r.stderr
    # US A (3), EU A (1) -> card 0 holds 3, card 1 gets EU A then US B
    assert env.card(1).exists()
    assert "ensure 1 rc=0 has=1 store=1" in r.stdout


def test_store_dir_is_created_when_missing(env, tmp_path):
    fresh = tmp_path / "fresh" / "profile"
    fresh.parent.mkdir()
    r = env.run("us", "ensure:0", store_dir=str(fresh))
    assert r.returncode == 0 and (fresh / "imported.txt").exists()
