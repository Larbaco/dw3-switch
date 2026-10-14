import csv
import json
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "extract_textures.py"
sys.path.insert(0, str(ROOT / "tools"))

import extract_textures as et  # noqa: E402


def tim4(w_px=8, h=2, pals=2):
    """A 4-bit TIM: `pals` 16-colour palettes, index = x % 16."""
    clut = b"".join(struct.pack("<H", (p * 16 + i) or 0) for p in range(pals) for i in range(16))
    clut_block = struct.pack("<IHHHH", 12 + len(clut), 0, 480, 16, pals) + clut
    row = bytes(((x + 1) % 16) << 4 | (x % 16) for x in range(0, w_px, 2))
    pix = row * h
    img = struct.pack("<IHHHH", 12 + len(pix), 640, 0, w_px // 4, h) + pix
    return b"\x10\x00\x00\x00" + struct.pack("<I", 8) + clut_block + img


def tim16(w=3, h=2):
    pix = struct.pack(f"<{w * h}H", *([0x7FFF, 0x001F, 0x0000] * h))
    img = struct.pack("<IHHHH", 12 + len(pix), 0, 0, w, h) + pix
    return b"\x10\x00\x00\x00" + struct.pack("<I", 2) + img


def rlen(data):
    out = bytearray(b"RLEN" + struct.pack("<I", len(data)))
    for i in range(0, len(data), 0x7F):
        chunk = data[i : i + 0x7F]
        out += bytes((len(chunk),)) + chunk
    return bytes(out) + b"\x00"


def archive(*entries):
    table = 4 * (len(entries) + 1)
    offs, body = [], b""
    for e in entries:
        offs.append(table + len(body))
        body += e + b"\x00" * (-len(e) % 4)
    return struct.pack(f"<{len(entries) + 1}I", *offs, 0) + body


def archive_sized(*entries):
    table = 4 * len(entries)
    offs, body = [], b""
    for e in entries:
        offs.append(table + len(body))
        body += e + b"\x00" * (-len(e) % 4)
    return struct.pack(f"<{len(entries)}I", *offs) + body


def read_png(path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    w, h = struct.unpack(">II", data[16:24])
    idat = data[data.index(b"IDAT") + 4 :]
    raw = zlib.decompress(idat[: struct.unpack(">I", data[data.index(b"IDAT") - 4 : data.index(b"IDAT")])[0]])
    return w, h, raw


def test_parse_and_rlen_roundtrip():
    t = tim4()
    tim = et.parse_tim(t, 0)
    assert tim and tim.width == 8 and tim.height == 2 and tim.mode == 0
    assert et.rlen_unpack(rlen(t)) == t
    assert et.parse_tim(t[:-1], 0) is None


def test_runtime_tree(tmp_path):
    rt = tmp_path / "runtime"
    (rt / "AAA" / "DAT").mkdir(parents=True)
    (rt / "AAA" / "STR").mkdir(parents=True)
    (rt / "AAA" / "DAT" / "PLAIN.TIM").write_bytes(tim16())
    # archive > RLEN > archive > TIM, like a field map tile
    tile = archive(tim4(), b"sprites!")
    (rt / "AAA" / "DAT" / "MAP.BIN").write_bytes(archive(rlen(tile), b"\x01\x02\x03\x04"))
    # TIMs inside other data, like the card images
    (rt / "AAA" / "DAT" / "CARDS.BIN").write_bytes(b"\xAA" * 12 + tim4() + b"\xBB" * 12 + tim4())
    (rt / "AAA" / "STR" / "MOVIE.STR").write_bytes(tim16())
    # disc image next to the tree: skipped
    (rt / "disc.bin").write_bytes(tim16())
    (rt / "disc.cue").write_bytes(b"FILE disc.bin BINARY")
    # title-logo style: table sized by its first offset, no 0 word, and a
    # TIM archive entry with an RLEN TIM
    (rt / "AAA" / "DAT" / "LOGO.BIN").write_bytes(
        archive_sized(b"\x34\x00\x00\x00sprites", archive(rlen(tim16()))))
    (rt / "files_us.json").write_text(json.dumps(
        {"files": {"7": {"path": "AAA/DAT/MAP.BIN"}}}), encoding="utf-8")

    out = tmp_path / "out"
    r = subprocess.run([sys.executable, str(TOOL), "--runtime", str(rt), "--out", str(out),
                        "--all-palettes", "--quiet"], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    rows = list(csv.DictReader((out / "manifest.csv").open()))
    pngs = sorted(row["png"] for row in rows)
    assert pngs == sorted([
        "AAA/DAT/CARDS.BIN/@00000c.png",
        "AAA/DAT/CARDS.BIN/@00000c.p1.png",
        "AAA/DAT/CARDS.BIN/@000080.png",
        "AAA/DAT/CARDS.BIN/@000080.p1.png",
        "AAA/DAT/LOGO.BIN/e001_e000.png",
        "AAA/DAT/MAP.BIN/e000_e000.png",
        "AAA/DAT/MAP.BIN/e000_e000.p1.png",
        "AAA/DAT/PLAIN.TIM/root.png",
    ])
    map_row = next(r for r in rows if r["png"] == "AAA/DAT/MAP.BIN/e000_e000.png")
    assert map_row["file_id"] == "7" and map_row["via"] == "archive>rlen>archive"
    assert map_row["palettes"] == "2" and map_row["vram_x"] == "640"

    w, h, raw = read_png(out / "AAA/DAT/PLAIN.TIM/root.png")
    assert (w, h) == (3, 2)
    assert raw[1:13] == bytes((255, 255, 255, 255, 255, 0, 0, 255, 0, 0, 0, 0))
    summary = json.loads((out / "summary.json").read_text())
    assert summary["tims"] == 5 and summary["unique_tims"] == 2
