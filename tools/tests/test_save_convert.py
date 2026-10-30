"""P6.25 cross-region save conversion and import (slice 1).

Everything here is synthetic: the saves are generated at test time from the
layout in vendor/dw3_decomp/include/engine/game_state.h (parsed when the
submodule is present) and live in pytest temp directories. No game data and no
save bytes are committed.
"""

from __future__ import annotations

import ctypes
import hashlib
import random
import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
CONVERT_C = ROOT / "port" / "memcard" / "save_convert.c"
HOST_C = ROOT / "port" / "memcard" / "memcard_host.c"
STORE_C = ROOT / "port" / "memcard" / "save_store.c"
TOOL_C = ROOT / "tools" / "save_convert_tool.c"
IMPORT_C = ROOT / "tools" / "tests" / "cross_import_main.c"
GAME_STATE_H = ROOT / "vendor" / "dw3_decomp" / "include" / "engine" / "game_state.h"
INC = ROOT / "port" / "memcard"

IMAGE = 0x8000
INFO_OFF, INFO_SIZE, SUMMARY = 0x200, 0xD4, 0x44
DATA_OFF, STRIDE = 0x300, 0x2700
SIZE = {"us": 0x26BC, "eu": 0x26C4}
VERSION = {"us": 3, "eu": 4}
FLAGS_OFF = 0x2644
PROGRESS_OFF = 0x263C
REGIONS = {"us": 1, "eu": 2}

# Independent copy of the table in docs/CROSS_REGION_SAVES.md (the test below
# compares both with the decomp header and with the C table).
GROUPS = {
    0x02: ((0x2644, 0xD), (0x2644, 0x12)),
    0x04: ((0x2651, 2), (0x2656, 2)),
    0x06: ((0x2653, 1), (0x2658, 1)),
    0x08: ((0x2654, 1), (0x2659, 1)),
    0x0A: ((0x2655, 2), (0x265A, 4)),
    0x0C: ((0x2657, 8), (0x265E, 8)),
    0x0E: ((0x265F, 0xC), (0x2666, 0xC)),
    0x10: ((0x266B, 2), (0x2672, 4)),
    0x18: ((0x266D, 1), (0x2676, 2)),
    0x1A: ((0x266E, 9), (0x2678, 9)),
    0x1C: ((0x2677, 0xB), (0x2681, 0xB)),
    0x20: ((0x2682, 0x1E), (0x268C, 0x1E)),
    0x40: ((0x26A0, 0x1C), (0x26AA, 0x1A)),
}


def xor(b: bytes) -> int:
    x = 0
    for v in b:
        x ^= v
    return x


def gcc(out: Path, *srcs: Path, shared: bool = False) -> None:
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-Wno-stringop-truncation", f"-I{INC}"]
    if shared:
        cmd += ["-shared", "-fPIC"]
    cmd += ["-o", str(out), *map(str, srcs)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout


@pytest.fixture(scope="module")
def lib(tmp_path_factory):
    d = tmp_path_factory.mktemp("saveconv_lib")
    so = d / "libsave_convert.so"
    gcc(so, CONVERT_C, shared=True)
    L = ctypes.CDLL(str(so))
    L.save_convert_slot.restype = ctypes.c_int
    L.save_convert_slot.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                    ctypes.POINTER(ctypes.c_uint),
                                    ctypes.POINTER(ctypes.c_int)]
    return L


@pytest.fixture(scope="module")
def tool(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("saveconv_tool") / "save_convert_tool"
    gcc(out, TOOL_C, CONVERT_C)
    return out


@pytest.fixture(scope="module")
def importer(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("saveconv_import") / "cross_import_main"
    gcc(out, IMPORT_C, HOST_C, CONVERT_C, STORE_C)
    return out


def conv_slot(lib, data: bytes, dst: str):
    out = ctypes.create_string_buffer(STRIDE)
    warn = ctypes.c_uint(0)
    src = ctypes.c_int(0)
    rc = lib.save_convert_slot(data, out, REGIONS[dst], ctypes.byref(warn),
                               ctypes.byref(src))
    return rc, bytes(out.raw), warn.value, src.value


def make_slot(region: str, seed: int, progress: int = 5, flags: bytes | None = None,
              tail_noise: bool = True) -> bytearray:
    """A well-formed data slot of `region` filled with seeded noise."""
    rng = random.Random(seed)
    size = SIZE[region]
    s = bytearray(rng.randrange(256) for _ in range(size))
    s[PROGRESS_OFF:PROGRESS_OFF + 4] = progress.to_bytes(4, "little")
    if flags is not None:
        s[FLAGS_OFF:FLAGS_OFF + len(flags)] = flags
    s[2] = VERSION[region]
    s[0] = xor(bytes(s[4:size]))
    tail = bytes(rng.randrange(256) for _ in range(STRIDE - size)) if tail_noise \
        else bytes(STRIDE - size)
    return s + tail


def zero_dropped(slot: bytearray, region: str) -> None:
    """Zero the bytes the other region has no place for, in a `region` slot."""
    ri, oi = (0, 1) if region == "us" else (1, 0)
    for (a, b) in GROUPS.values():
        mine, theirs = (a, b) if ri == 0 else (b, a)
        if mine[1] > theirs[1]:
            for k in range(theirs[1], mine[1]):
                slot[mine[0] + k] = 0
    size = SIZE[region]
    slot[0] = xor(bytes(slot[4:size]))
    del oi


def make_image(region: str, slots: list[bytearray | None], seed: int = 1) -> bytes:
    rng = random.Random(seed)
    img = bytearray(IMAGE)
    img[0:INFO_OFF] = bytes(rng.randrange(256) for _ in range(INFO_OFF))
    info = bytearray(DATA_OFF - INFO_OFF)
    for i in range(3):
        base = 8 + i * SUMMARY
        if slots[i] is not None:
            info[base:base + 8] = b"Tester%02d" % i
            for k in range(0x18 + 8, SUMMARY):
                info[base + k] = rng.randrange(256)
    info[4:8] = (0x33574D44).to_bytes(4, "little")
    info[1] = 1
    info[2] = VERSION[region]
    for k in range(INFO_SIZE, len(info)):
        info[k] = rng.randrange(256)
    info[0] = xor(bytes(info[4:INFO_SIZE]))
    img[INFO_OFF:DATA_OFF] = info
    for i in range(3):
        base = DATA_OFF + i * STRIDE
        if slots[i] is not None:
            img[base:base + STRIDE] = slots[i]
        else:
            img[base:base + STRIDE] = bytes(rng.randrange(256) for _ in range(STRIDE))
    return bytes(img)


def bit_get(slot: bytes, group: int, region: str, bit: int) -> int:
    off, size = GROUPS[group][0 if region == "us" else 1]
    assert bit < size * 8
    return (slot[off + bit // 8] >> (bit % 8)) & 1


# ---------------------------------------------------------------------- table

def test_c_table_matches_independent_table(tool: Path):
    out = subprocess.run([str(tool), "table"], capture_output=True, text=True,
                         check=True).stdout.splitlines()
    got = {}
    for line in out:
        if line.startswith("sizes"):
            parts = line.split()
            assert int(parts[1], 16) == SIZE["us"] and int(parts[2], 16) == SIZE["eu"]
            assert (int(parts[4]), int(parts[5])) == (3, 4)
            continue
        g, uo, us, eo, es = (int(x, 16) for x in line.split())
        got[g] = ((uo, us), (eo, es))
    assert got == GROUPS


def _parse_header_layout():
    text = GAME_STATE_H.read_text()
    m = re.search(r"#if VERSION_US\n(\s*/\* 0x2644.*?)#elif VERSION_EU\n(.*?)#endif", text, re.S)
    assert m
    out = {}
    for region, body in (("us", m.group(1)), ("eu", m.group(2))):
        d = {}
        for off, name, size in re.findall(
                r"/\* 0x([0-9A-F]+) \*/ u8 flags([0-9A-F]{2})\[0x([0-9A-F]+)\];", body):
            d[int(name, 16)] = (int(off, 16), int(size, 16))
        out[region] = d
    return out, text


@pytest.mark.skipif(not GAME_STATE_H.is_file(), reason="decomp submodule not present")
def test_table_matches_decomp_header():
    layout, text = _parse_header_layout()
    assert sorted(layout["us"]) == sorted(GROUPS) == sorted(layout["eu"])
    for g, (us, eu) in GROUPS.items():
        assert layout["us"][g] == us, hex(g)
        assert layout["eu"][g] == eu, hex(g)
    m = re.search(r"#if VERSION_US\n#define GAME_SAVE_SIZE (0x[0-9A-Fa-f]+)\n"
                  r"#elif VERSION_EU\n#define GAME_SAVE_SIZE (0x[0-9A-Fa-f]+)", text)
    assert m and (int(m.group(1), 16), int(m.group(2), 16)) == (SIZE["us"], SIZE["eu"])
    # the common block is 0x2644 bytes and both layouts start flags there
    assert layout["us"][0x02][0] == layout["eu"][0x02][0] == FLAGS_OFF
    # progress sits before the flags in both
    assert re.search(r"/\* 0x263C \*/ s32 progress;", text)
    mc = ROOT / "vendor" / "dw3_decomp" / "include" / "menus" / "memory_card.h"
    t = mc.read_text()
    assert re.search(r"#if VERSION_US\n#define MEMCARD_SAVE_VERSION 3\n"
                     r"#elif VERSION_EU\n#define MEMCARD_SAVE_VERSION 4", t)


# ------------------------------------------------------------------ slot level

@pytest.mark.parametrize("src,dst", [("us", "eu"), ("eu", "us")])
def test_every_flag_bit_moves_by_group_and_id(lib, src, dst):
    """Each bit of each group, set alone, lands on the same (group, id)."""
    si = 0 if src == "us" else 1
    for g, pair in GROUPS.items():
        off, size = pair[si]
        d_off, d_size = pair[1 - si]
        for bit in range(size * 8):
            flags = bytearray(0x80)
            flags[off - FLAGS_OFF + bit // 8] = 1 << (bit % 8)
            slot = make_slot(src, seed=7, flags=bytes(flags))
            rc, out, warn, sr = conv_slot(lib, bytes(slot), dst)
            assert rc == 2 and sr == REGIONS[src], (hex(g), bit)
            if bit < d_size * 8:
                assert bit_get(out, g, dst, bit) == 1, (hex(g), bit)
                assert warn == 0
            else:
                assert warn == 1, (hex(g), bit)  # dropped bit is reported
            # no other flag bit is set in the output
            total = sum(bin(b).count("1") for b in out[FLAGS_OFF:SIZE[dst]])
            assert total == (1 if bit < d_size * 8 else 0), (hex(g), bit)


@pytest.mark.parametrize("src,dst", [("us", "eu"), ("eu", "us")])
def test_common_block_version_checksum_tail(lib, src, dst):
    slot = make_slot(src, seed=11)
    rc, out, warn, _ = conv_slot(lib, bytes(slot), dst)
    assert rc == 2
    # bytes 0x0000-0x2643 identical except the checksum (0) and version (2)
    assert out[1] == slot[1] and out[3:FLAGS_OFF] == slot[3:FLAGS_OFF]
    assert out[2] == VERSION[dst]
    # exact XOR over [4, GAME_SAVE_SIZE) of the target region
    assert out[0] == xor(out[4:SIZE[dst]])
    # the runtime tail is don't-care: the converter clears it
    assert out[SIZE[dst]:] == bytes(STRIDE - SIZE[dst])
    # the pad bytes of byte 1 and 3 are carried
    assert out[3] == slot[3]


@pytest.mark.parametrize("a,b", [("us", "eu"), ("eu", "us")])
def test_round_trip_identity_when_nothing_dropped(lib, a, b):
    slot = make_slot(a, seed=21, progress=7, tail_noise=False)
    zero_dropped(slot, a)
    rc, mid, _, _ = conv_slot(lib, bytes(slot), b)
    assert rc == 2
    rc, back, warn, _ = conv_slot(lib, mid, a)
    assert rc == 2 and warn == 0
    assert back[:SIZE[a]] == bytes(slot[:SIZE[a]])


def test_us_to_eu_to_us_is_identity_over_saved_bytes_even_with_dropped_flags40(lib):
    # US flags40 bytes 0x1A-0x1B have no place in EU: the copy loses them and
    # says so (warn bit); the original is untouched, which is the safety net.
    slot = make_slot("us", seed=31, tail_noise=False)
    rc, mid, warn, _ = conv_slot(lib, bytes(slot), "eu")
    assert rc == 2
    expect_warn = 1 if (slot[0x26A0 + 0x1A] or slot[0x26A0 + 0x1B]) else 0
    assert warn == expect_warn
    rc, back, _, _ = conv_slot(lib, mid, "us")
    assert back[4:0x26A0 + 0x1A] == bytes(slot[4:0x26A0 + 0x1A])


def test_native_slot_copied_as_is(lib):
    slot = make_slot("eu", seed=41)
    rc, out, warn, sr = conv_slot(lib, bytes(slot), "eu")
    assert rc == 1 and out == bytes(slot) and sr == 2 and warn == 0


def test_extra_chapter_refused_in_us_bytes_kept(lib):
    slot = make_slot("eu", seed=51, progress=0x2D)
    rc, out, _, _ = conv_slot(lib, bytes(slot), "us")
    assert rc == 3 and out == bytes(slot) and out[2] == 4  # US game: error 8
    slot = make_slot("eu", seed=52, progress=0x2C)
    rc, _, _, _ = conv_slot(lib, bytes(slot), "us")
    assert rc == 2
    # EU accepts a US save at any progress
    slot = make_slot("us", seed=53, progress=0x2C)
    rc, _, _, _ = conv_slot(lib, bytes(slot), "eu")
    assert rc == 2


def test_invalid_slots_kept_verbatim_never_repaired(lib):
    slot = make_slot("us", seed=61)
    bad_sum = bytearray(slot)
    bad_sum[0] ^= 0x55
    rc, out, _, sr = conv_slot(lib, bytes(bad_sum), "eu")
    assert rc == 4 and out == bytes(bad_sum) and sr == 0
    bad_ver = bytearray(slot)
    bad_ver[2] = 9
    bad_ver[0] = xor(bytes(bad_ver[4:SIZE["us"]]))
    rc, out, _, _ = conv_slot(lib, bytes(bad_ver), "eu")
    assert rc == 4 and out == bytes(bad_ver)
    rc, out, _, _ = conv_slot(lib, bytes(STRIDE), "us")
    assert rc == 4 and out == bytes(STRIDE)
    # a US-version slot with the EU-length checksum is not valid either
    mixed = bytearray(slot)
    mixed[0] = xor(bytes(mixed[4:SIZE["eu"]]))
    if mixed[0] != slot[0]:
        rc, _, _, _ = conv_slot(lib, bytes(mixed), "eu")
        assert rc == 4


# ----------------------------------------------------------------- image level

def run_tool(tool, *args):
    return subprocess.run([str(tool), *map(str, args)], capture_output=True, text=True)


@pytest.mark.parametrize("src,dst", [("us", "eu"), ("eu", "us")])
def test_image_convert_file_level(tool, tmp_path, src, dst):
    slots = [make_slot(src, 100), None, make_slot(src, 102)]
    for s in slots:
        if s is not None:
            zero_dropped(s, src)
    img = make_image(src, slots)
    inp, out = tmp_path / "save0.bin", tmp_path / "conv.bin"
    inp.write_bytes(img)
    before = hashlib.sha256(img).hexdigest()
    r = run_tool(tool, "convert", inp, out, dst)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "converter=1" in r.stdout and "results=2,0,2" in r.stdout
    conv = out.read_bytes()
    assert len(conv) == IMAGE
    assert hashlib.sha256(inp.read_bytes()).hexdigest() == before  # source untouched
    # header and icon sectors verbatim
    assert conv[:INFO_OFF] == img[:INFO_OFF]
    # info: names/summaries verbatim, version and checksum for the target
    info = conv[INFO_OFF:DATA_OFF]
    assert info[8:INFO_SIZE] == img[INFO_OFF + 8:INFO_OFF + INFO_SIZE]
    assert info[INFO_SIZE:] == img[INFO_OFF + INFO_SIZE:DATA_OFF]
    assert info[2] == VERSION[dst]
    assert info[0] == xor(info[4:INFO_SIZE])
    assert info[4:8] == (0x33574D44).to_bytes(4, "little")
    assert info[8:8 + 8] == b"Tester00" and info[8 + SUMMARY] == 0
    assert info[8 + 2 * SUMMARY:8 + 2 * SUMMARY + 8] == b"Tester02"
    # unused slot 1 data is cleared, converted slots are valid for the target
    assert conv[DATA_OFF + STRIDE:DATA_OFF + 2 * STRIDE] == bytes(STRIDE)
    for i in (0, 2):
        s = conv[DATA_OFF + i * STRIDE:DATA_OFF + (i + 1) * STRIDE]
        assert s[2] == VERSION[dst] and s[0] == xor(s[4:SIZE[dst]])
    # and back again: the original saved bytes come back
    back = tmp_path / "back.bin"
    r = run_tool(tool, "convert", out, back, src)
    assert r.returncode == 0
    bk = back.read_bytes()
    for i in (0, 2):
        o = DATA_OFF + i * STRIDE
        assert bk[o:o + SIZE[src]] == img[o:o + SIZE[src]]
    assert bk[INFO_OFF:DATA_OFF][:INFO_SIZE] == img[INFO_OFF:INFO_OFF + INFO_SIZE]


def test_tool_never_overwrites(tool, tmp_path):
    img = make_image("us", [make_slot("us", 1), None, None])
    inp, out = tmp_path / "in.bin", tmp_path / "out.bin"
    inp.write_bytes(img)
    out.write_bytes(b"precious")
    r = run_tool(tool, "convert", inp, out, "eu")
    assert r.returncode == 4 and out.read_bytes() == b"precious"
    r = run_tool(tool, "convert", inp, inp, "eu")
    assert r.returncode == 4 and inp.read_bytes() == img


@pytest.mark.parametrize("bad", ["short", "long", "zeros", "noise", "info_checksum",
                                 "info_magic"])
def test_unknown_or_corrupt_image_fails_closed(tool, tmp_path, bad):
    img = bytearray(make_image("us", [make_slot("us", 1), None, None]))
    if bad == "short":
        data = bytes(img[:IMAGE - 1])
    elif bad == "long":
        data = bytes(img) + b"\0"
    elif bad == "zeros":
        data = bytes(IMAGE)
    elif bad == "noise":
        rng = random.Random(5)
        data = bytes(rng.randrange(256) for _ in range(IMAGE))
    elif bad == "info_checksum":
        img[INFO_OFF] ^= 0x01
        data = bytes(img)
    else:
        img[INFO_OFF + 4] ^= 0x01
        img[INFO_OFF] = xor(bytes(img[INFO_OFF + 4:INFO_OFF + INFO_SIZE]))
        data = bytes(img)
    inp, out = tmp_path / "in.bin", tmp_path / "out.bin"
    inp.write_bytes(data)
    r = run_tool(tool, "convert", inp, out, "eu")
    assert r.returncode in (2, 3), r.stdout + r.stderr
    assert not out.exists()
    assert inp.read_bytes() == data


def test_image_with_only_unusable_slots_writes_nothing(tool, tmp_path):
    extra = make_slot("eu", 9, progress=0x2D)
    bad = bytearray(make_slot("eu", 10))
    bad[0] ^= 0xFF
    img = make_image("eu", [extra, bad, None])
    inp, out = tmp_path / "in.bin", tmp_path / "out.bin"
    inp.write_bytes(img)
    r = run_tool(tool, "convert", inp, out, "us")
    assert r.returncode == 3 and not out.exists()
    assert "results=3,4,0" in r.stdout


def test_mixed_image_keeps_unusable_slot_bytes(tool, tmp_path):
    extra = make_slot("eu", 9, progress=0x2D)
    ok = make_slot("eu", 12, progress=3)
    img = make_image("eu", [extra, ok, None])
    inp, out = tmp_path / "in.bin", tmp_path / "out.bin"
    inp.write_bytes(img)
    r = run_tool(tool, "convert", inp, out, "us")
    assert r.returncode == 0 and "results=3,2,0" in r.stdout
    conv = out.read_bytes()
    assert conv[DATA_OFF:DATA_OFF + STRIDE] == img[DATA_OFF:DATA_OFF + STRIDE]
    assert conv[DATA_OFF + STRIDE + 2] == 3


# ------------------------------------------------------------------ host import

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run_import(importer, *args, env=None):
    import os
    e = dict(os.environ)
    e.pop("DW3_CROSS_IMPORT", None)
    e.pop("DW3_MEMCARD_IMPORT_DIR", None)
    e.update(env or {})
    return subprocess.run([str(importer), *map(str, args)], capture_output=True,
                          text=True, env=e)


def setup_dirs(tmp_path, src_region, slots):
    foreign = tmp_path / "foreign"
    own = tmp_path / "own"
    foreign.mkdir()
    own.mkdir()
    img = make_image(src_region, slots)
    (foreign / "save0.bin").write_bytes(img)
    return foreign, own, img


@pytest.mark.parametrize("src,dst", [("us", "eu"), ("eu", "us")])
def test_import_creates_converted_copy_and_keeps_original(importer, tmp_path, src, dst):
    s = make_slot(src, 70)
    zero_dropped(s, src)
    foreign, own, img = setup_dirs(tmp_path, src, [s, None, None])
    before = sha(foreign / "save0.bin")
    r = run_import(importer, "explicit", own, foreign, dst)
    assert r.returncode == 0, r.stderr
    assert "port 0 ensure=0 has=1" in r.stdout
    assert "port 1 ensure=0 has=0" in r.stdout
    assert "cross-import slot=0 from=%s to=%s converter=1" % (src, dst) in r.stderr
    assert sha(foreign / "save0.bin") == before
    assert not (foreign / "save0.import").exists()
    out = (own / "save0.bin").read_bytes()
    assert len(out) == IMAGE
    slot = out[DATA_OFF:DATA_OFF + STRIDE]
    assert slot[2] == VERSION[dst] and slot[0] == xor(slot[4:SIZE[dst]])
    marker = (own / "save0.import").read_text()
    assert marker.startswith("dw3-cross-region-import 1\n")
    assert "converter=1" in marker and ("from=%s" % src) in marker
    assert not (own / "save0.name").exists()  # naked: any DW3 product name matches
    assert not (own / "save1.bin").exists()
    # a second start does nothing and rewrites nothing
    h = sha(own / "save0.bin")
    r = run_import(importer, "explicit", own, foreign, dst)
    assert "cross-import slot=0" not in r.stderr and sha(own / "save0.bin") == h


def test_import_never_overwrites_an_existing_save(importer, tmp_path):
    foreign, own, _ = setup_dirs(tmp_path, "us", [make_slot("us", 1), None, None])
    mine = make_image("eu", [make_slot("eu", 2), None, None], seed=99)
    (own / "save0.bin").write_bytes(mine)
    r = run_import(importer, "explicit", own, foreign, "eu")
    assert r.returncode == 0 and "port 0 ensure=0 has=1" in r.stdout
    assert (own / "save0.bin").read_bytes() == mine
    assert not (own / "save0.import").exists()


def test_import_does_not_come_back_after_the_save_is_deleted(importer, tmp_path):
    foreign, own, _ = setup_dirs(tmp_path, "us", [make_slot("us", 1), None, None])
    run_import(importer, "explicit", own, foreign, "eu")
    (own / "save0.bin").unlink()
    r = run_import(importer, "explicit", own, foreign, "eu")
    assert "port 0 ensure=0 has=0" in r.stdout and not (own / "save0.bin").exists()


@pytest.mark.parametrize("kind", ["noise", "short", "zeros", "dir"])
def test_import_corrupt_source_fails_closed(importer, tmp_path, kind):
    foreign = tmp_path / "foreign"
    own = tmp_path / "own"
    foreign.mkdir()
    own.mkdir()
    p = foreign / "save0.bin"
    rng = random.Random(3)
    if kind == "noise":
        p.write_bytes(bytes(rng.randrange(256) for _ in range(IMAGE)))
    elif kind == "short":
        p.write_bytes(make_image("us", [make_slot("us", 1), None, None])[:1000])
    elif kind == "zeros":
        p.write_bytes(bytes(IMAGE))
    else:
        p.mkdir()
    r = run_import(importer, "explicit", own, foreign, "eu")
    assert r.returncode == 0, r.stderr
    assert "port 0 ensure=0 has=0" in r.stdout
    assert sorted(x.name for x in own.iterdir()) == []
    if kind != "dir":
        assert p.exists()


def test_import_only_extra_chapter_creates_nothing(importer, tmp_path):
    foreign, own, img = setup_dirs(tmp_path, "eu", [make_slot("eu", 1, progress=0x2D),
                                                    None, None])
    r = run_import(importer, "explicit", own, foreign, "us")
    assert "port 0 ensure=0 has=0" in r.stdout
    assert list(own.iterdir()) == []
    assert (foreign / "save0.bin").read_bytes() == img


def test_import_both_ports(importer, tmp_path):
    foreign, own, _ = setup_dirs(tmp_path, "us", [make_slot("us", 1), None, None])
    (foreign / "save1.bin").write_bytes(
        make_image("us", [None, make_slot("us", 2), None], seed=4))
    r = run_import(importer, "explicit", own, foreign, "eu")
    assert "port 0 ensure=0 has=1" in r.stdout and "port 1 ensure=0 has=1" in r.stdout
    assert (own / "save1.import").exists()


def test_sibling_layout_and_names(importer, tmp_path):
    root = tmp_path / "dw3"
    (root / "us" / "memcard").mkdir(parents=True)
    (root / "eu" / "memcard").mkdir(parents=True)
    img = make_image("us", [make_slot("us", 1), None, None])
    (root / "us" / "memcard" / "save0.bin").write_bytes(img)
    r = run_import(importer, "sibling", root / "eu", "eu")
    assert r.returncode == 0 and "port 0 ensure=0 has=1" in r.stdout
    assert (root / "eu" / "memcard" / "save0.bin").is_file()
    assert (root / "us" / "memcard" / "save0.bin").read_bytes() == img
    # the reverse direction of a copy made by the EU build
    r = run_import(importer, "sibling-rc", root / "us", "us")
    assert r.stdout.strip() == "rc=0"
    for bad, region in ((root / "other", "eu"), (root / "us", "eu"), (root / "eu", "us"),
                        (tmp_path / "x", "us")):
        r = run_import(importer, "sibling-rc", bad, region)
        assert r.stdout.strip() == "rc=-1", (bad, region)


def test_env_switches(importer, tmp_path):
    foreign, own, _ = setup_dirs(tmp_path, "us", [make_slot("us", 1), None, None])
    root = tmp_path / "dw3" / "eu"
    # explicit memcard dir (replay runs set DW3_MEMCARD_DIR): no import ...
    r = run_import(importer, "env", own, root, "eu", 1)
    assert "setup=-1" in r.stdout and "has=0" in r.stdout
    # ... unless the import folder is named too
    r = run_import(importer, "env", own, root, "eu", 1,
                   env={"DW3_MEMCARD_IMPORT_DIR": str(foreign)})
    assert "setup=0" in r.stdout and "port 0 ensure=0 has=1" in r.stdout
    # DW3_CROSS_IMPORT=0 wins over everything
    own2 = tmp_path / "own2"
    own2.mkdir()
    r = run_import(importer, "env", own2, root, "eu", 1,
                   env={"DW3_MEMCARD_IMPORT_DIR": str(foreign), "DW3_CROSS_IMPORT": "0"})
    assert "setup=-1" in r.stdout and "has=0" in r.stdout
    assert list(own2.iterdir()) == []
