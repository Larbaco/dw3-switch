"""US and EU runtime extraction from synthetic Mode2/2352 images.

The payload files are 2 and 3 sectors. The image is larger only because a
PS-X EXE has to reach the selected version's STAGE_VRAM tail. No game bytes.
"""

import json
import os
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SECTOR = 2352
USER = 24
LOGICAL = 2048
EXE_LOAD = 0x80010000
STAGE_VRAM = {"us": 0x800A4CA4, "eu": 0x800A5DE0}
EXECUTABLE = {"us": "SLUS_014.36", "eu": "SLES_039.36"}
EXE_HEADER = 0x800


def _both32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def _both16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def _mode2(user: bytes = b"") -> bytes:
    user = user.ljust(LOGICAL, b"\x00")[:LOGICAL]
    sync = b"\x00" + b"\xff" * 10 + b"\x00"
    header = b"\x00\x00\x00\x02"
    sub = bytes([0, 0, 0x08, 0, 0, 0, 0x08, 0])
    tail = b"\x00" * (SECTOR - USER - LOGICAL)
    return sync + header + sub + user + tail


def _dir_record(name: bytes, lba: int, size: int, *, is_dir: bool = False) -> bytes:
    flags = 0x02 if is_dir else 0x00
    body = bytearray()
    body.append(0)
    body.append(0)
    body += _both32(lba)
    body += _both32(size)
    body += b"\x00" * 7
    body.append(flags)
    body.append(0)
    body.append(0)
    body += _both16(1)
    body.append(len(name))
    body += name
    if (len(name) & 1) == 0:
        body.append(0)
    body[0] = len(body)
    return bytes(body)


def _path_entry(name: bytes, extent: int, parent: int) -> bytes:
    entry = bytearray()
    entry.append(len(name))
    entry.append(0)
    entry += struct.pack("<I", extent)
    entry += struct.pack("<H", parent)
    entry += name
    if len(name) & 1:
        entry.append(0)
    return bytes(entry)


def _build_exe(version: str) -> tuple[bytes, bytes]:
    """PS-X EXE with a CLUT TIM at this version's stage address."""
    off = EXE_HEADER + (STAGE_VRAM[version] - EXE_LOAD)
    flag = 8
    clut_size = 16
    img_size = 20
    exe = bytearray(off + 8 + clut_size + img_size)
    exe[:8] = b"PS-X EXE"
    exe[off : off + 4] = b"\x10\x00\x00\x00"
    struct.pack_into("<I", exe, off + 4, flag)
    struct.pack_into("<I", exe, off + 8, clut_size)
    exe[off + 12 : off + 8 + clut_size] = b"CLUT" + b"\x11" * (clut_size - 8)
    pos = off + 8 + clut_size
    struct.pack_into("<I", exe, pos, img_size)
    exe[pos + 4 : pos + img_size] = b"BOOTTIM!" + b"\x5a" * (img_size - 12)
    tim = bytes(exe[off : pos + img_size])
    return bytes(exe), tim


def _build_image(dest: Path, version: str = "us") -> dict:
    exe, tim = _build_exe(version)
    demo = bytes((i * 17 + 3) & 0xFF for i in range(4200))
    assert len(demo) == 4200
    exe_lba = 40
    exe_sectors = (len(exe) + LOGICAL - 1) // LOGICAL
    demo_lba = exe_lba + exe_sectors
    demo_sectors = 3
    str_lba = demo_lba + demo_sectors
    str_sectors = 2
    volume = str_lba + str_sectors

    root_lba, aaa_lba, pro_lba, str_dir_lba = 19, 20, 21, 22
    pt_lba = 18

    def directory(records: list[bytes], self_lba: int, parent_lba: int) -> bytes:
        payload = b"".join(records)
        dot = _dir_record(b"\x00", self_lba, len(payload), is_dir=True)
        dotdot = _dir_record(b"\x01", parent_lba, len(payload), is_dir=True)
        # Rebuild so "." carries the final length (record size does not change).
        body = dot + dotdot + b"".join(records)
        dot = _dir_record(b"\x00", self_lba, len(body), is_dir=True)
        dotdot = _dir_record(b"\x01", parent_lba, len(body), is_dir=True)
        return dot + dotdot + b"".join(records)

    pro = directory(
        [_dir_record(b"DEMO.PRO;1", demo_lba, len(demo))],
        pro_lba,
        aaa_lba,
    )
    str_dir = directory(
        [_dir_record(b"CLIP.STR;1", str_lba, str_sectors * LOGICAL)],
        str_dir_lba,
        aaa_lba,
    )
    aaa = directory(
        [
            _dir_record(b"PRO", pro_lba, len(pro), is_dir=True),
            _dir_record(b"STR", str_dir_lba, len(str_dir), is_dir=True),
        ],
        aaa_lba,
        root_lba,
    )
    root = directory(
        [
            _dir_record(b"AAA", aaa_lba, len(aaa), is_dir=True),
            _dir_record(f"{EXECUTABLE[version]};1".encode(), exe_lba, len(exe)),
        ],
        root_lba,
        root_lba,
    )
    # The "." size must describe the directory that contains it. directory()
    # computes that. Re-nesting above used len() of the child blob, which is
    # the on-disc directory size. Good.

    path_table = b"".join(
        [
            _path_entry(b"\x00", root_lba, 1),
            _path_entry(b"AAA", aaa_lba, 1),
            _path_entry(b"PRO", pro_lba, 2),
            _path_entry(b"STR", str_dir_lba, 2),
        ]
    )
    pvd = bytearray(LOGICAL)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[80:88] = _both32(volume)
    pvd[120:124] = _both16(LOGICAL)
    pvd[132:136] = struct.pack("<I", len(path_table))
    pvd[140:144] = struct.pack("<I", pt_lba)
    root_rec = _dir_record(b"\x00", root_lba, len(root), is_dir=True)
    assert len(root_rec) == 34
    pvd[156:190] = root_rec

    image = bytearray(volume * SECTOR)
    for i in range(volume):
        image[i * SECTOR : (i + 1) * SECTOR] = _mode2()

    def poke(lba: int, payload: bytes) -> None:
        for i, byte in enumerate(payload):
            sec = lba + i // LOGICAL
            image[sec * SECTOR + USER + (i % LOGICAL)] = byte

    poke(16, bytes(pvd))
    poke(pt_lba, path_table)
    poke(root_lba, root)
    poke(aaa_lba, aaa)
    poke(pro_lba, pro)
    poke(str_dir_lba, str_dir)
    poke(exe_lba, exe)
    poke(demo_lba, demo)

    str_raw = bytearray(str_sectors * SECTOR)
    for s in range(str_sectors):
        str_raw[s * SECTOR : (s + 1) * SECTOR] = _mode2()
    str_raw[USER + 2100] = 0xA5
    str_raw[SECTOR + USER + 2100] = 0x5A
    image[str_lba * SECTOR : (str_lba + str_sectors) * SECTOR] = str_raw

    dest.write_bytes(image)
    return {
        "exe": exe,
        "tim": tim,
        "demo": demo,
        "str_raw": bytes(str_raw),
        "demo_lba": demo_lba,
        "str_lba": str_lba,
        "demo_sectors": demo_sectors,
        "str_sectors": str_sectors,
        "str_iso_bytes": str_sectors * LOGICAL,
        "version": version,
        "executable": EXECUTABLE[version],
    }


def _compile(tmp: Path, version: str = "us") -> Path:
    exe = tmp / f"runtime_extract_test_{version}"
    if exe.exists():
        return exe
    subprocess.check_call(
        [
            "cc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-D_FILE_OFFSET_BITS=64",
            f"-DVERSION_{version.upper()}=1",
            "-I",
            str(ROOT / "port" / "cdfs"),
            "-I",
            str(ROOT / "port" / "movies"),
            "-o",
            str(exe),
            str(ROOT / "port" / "cdfs" / "runtime_extract.c"),
            str(ROOT / "port" / "cdfs" / "cdfs.c"),
            str(ROOT / "port" / "movies" / "str_disc.c"),
            str(ROOT / "tools" / "tests" / "runtime_extract_main.c"),
        ]
    )
    return exe


def _run(
    tmp: Path, runtime: Path, table: Path, version: str = "us"
) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.pop("DW3_DISC_BIN", None)
    env.pop("DW3_DISC_CUE", None)
    return subprocess.run(
        [str(_compile(tmp, version)), str(runtime), str(table)],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )


def _write_table(path: Path, rows: list[tuple[int, int, int]]) -> None:
    path.write_text("".join(f"{i} {lba} {n}\n" for i, lba, n in rows), encoding="utf-8")


def _image_rows(info: dict) -> list[tuple[int, int, int]]:
    return [
        (1, info["demo_lba"], info["demo_sectors"]),
        (2, info["str_lba"], info["str_sectors"]),
    ]


def test_extracts_form1_user_form2_raw_and_boot_tim(tmp_path: Path):
    card = tmp_path / "card"
    runtime = card / "us"
    runtime.mkdir(parents=True)
    info = _build_image(card / "Digimon World 3.bin")
    table = tmp_path / "table.txt"
    _write_table(
        table,
        _image_rows(info),
    )

    first = _run(tmp_path, runtime, table)
    assert first.returncode == 0, first.stderr + first.stdout
    assert "extracting runtime from" in first.stdout
    assert "TICK" in first.stdout
    assert "MSG extracting runtime" in first.stdout

    demo = (runtime / "AAA" / "PRO" / "DEMO.PRO").read_bytes()
    assert demo == info["demo"]
    assert len(demo) == 4200

    clip = (runtime / "AAA" / "STR" / "CLIP.STR").read_bytes()
    assert clip == info["str_raw"]
    assert len(clip) == info["str_sectors"] * SECTOR
    assert clip[USER + 2100] == 0xA5
    assert clip[SECTOR + USER + 2100] == 0x5A

    assert (runtime / "SLUS_014.36").read_bytes() == info["exe"]
    assert (runtime / "boot_tim.bin").read_bytes() == info["tim"]
    assert info["tim"].startswith(b"\x10\x00\x00\x00")

    mapped = json.loads((runtime / "files_us.json").read_text(encoding="utf-8"))
    assert mapped["version"] == "us"
    assert mapped["executable"] == "SLUS_014.36"
    assert mapped["files"]["1"]["path"] == "AAA/PRO/DEMO.PRO"
    assert mapped["files"]["1"]["form"] == 1
    assert mapped["files"]["1"]["bytes"] == 4200
    assert mapped["files"]["1"]["sectors"] == 3
    assert mapped["files"]["2"]["path"] == "AAA/STR/CLIP.STR"
    assert mapped["files"]["2"]["form"] == 2
    assert mapped["files"]["2"]["bytes"] == info["str_iso_bytes"]
    assert mapped["files"]["2"]["sectors"] == 2
    assert "SLUS_014.36" not in json.dumps(mapped["files"])

    memcard = runtime / "memcard"
    assert memcard.is_dir()
    assert list(memcard.iterdir()) == []
    kept = memcard / "memcard0.mcr"
    kept.write_bytes(b"KEEP")
    (memcard / "memcard1.mcr").write_bytes(b"KEEP2")

    clip_mtime = (runtime / "AAA" / "STR" / "CLIP.STR").stat().st_mtime_ns
    (runtime / "AAA" / "PRO" / "DEMO.PRO").unlink()
    again = _run(tmp_path, runtime, table)
    assert again.returncode == 0, again.stderr + again.stdout
    assert (runtime / "AAA" / "PRO" / "DEMO.PRO").read_bytes() == info["demo"]
    assert (runtime / "AAA" / "STR" / "CLIP.STR").stat().st_mtime_ns == clip_mtime
    assert kept.read_bytes() == b"KEEP"
    assert (memcard / "memcard1.mcr").read_bytes() == b"KEEP2"

    (card / "Digimon World 3.bin").unlink()
    third = _run(tmp_path, runtime, table)
    assert third.returncode == 0, third.stderr + third.stdout
    assert "extracting runtime" not in third.stdout
    assert "TICK" not in third.stdout
    assert (runtime / "SLUS_014.36").read_bytes() == info["exe"]


def test_complete_packed_tree_only_stats(tmp_path: Path):
    runtime = tmp_path / "us"
    str_dir = runtime / "AAA" / "STR"
    str_dir.mkdir(parents=True)
    clip = str_dir / "CLIP.STR"
    clip.write_bytes(b"\x2a" * (2 * LOGICAL))
    exe = runtime / "SLUS_014.36"
    exe.write_bytes(b"PS-X EXE" + b"\x00" * EXE_HEADER)
    (runtime / "boot_tim.bin").write_bytes(b"\x10\x00\x00\x00" + b"\x00" * 12)
    (runtime / "files_us.json").write_text(
        json.dumps(
            {
                "version": "us",
                "executable": "SLUS_014.36",
                "files": {
                    "2": {
                        "path": "AAA/STR/CLIP.STR",
                        "lba": 10,
                        "sectors": 2,
                        "bytes": 2 * LOGICAL,
                        "form": 2,
                    }
                },
            }
        ),
        encoding="utf-8",
    )
    mem = runtime / "memcard"
    mem.mkdir()
    (mem / "memcard0.mcr").write_bytes(b"SAVE")
    table = tmp_path / "table.txt"
    table.write_text("", encoding="utf-8")

    result = _run(tmp_path, runtime, table)
    assert result.returncode == 0, result.stderr + result.stdout
    assert "extracting runtime" not in result.stdout
    assert clip.read_bytes() == b"\x2a" * (2 * LOGICAL)
    assert (mem / "memcard0.mcr").read_bytes() == b"SAVE"
    assert not (mem / "memcard1.mcr").exists()


def test_missing_tree_and_missing_disc_is_not_an_extract_error(tmp_path: Path):
    runtime = tmp_path / "empty" / "us"
    runtime.mkdir(parents=True)
    table = tmp_path / "table.txt"
    _write_table(table, [(1, 30, 2)])
    result = _run(tmp_path, runtime, table)
    assert result.returncode == 0, result.stderr + result.stdout
    assert not (runtime / "files_us.json").exists()
    assert "extracting runtime" not in result.stdout


def test_eu_extracts_matching_disc_and_reentry_only_validates(tmp_path: Path):
    runtime = tmp_path / "sd" / "switch" / "dw3" / "eu"
    runtime.mkdir(parents=True)
    disc = runtime / "disc.bin"
    info = _build_image(disc, version="eu")
    (runtime / "disc.cue").write_text(
        'FILE "disc.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n',
        encoding="ascii",
    )
    table = tmp_path / "eu-table.txt"
    _write_table(table, _image_rows(info))

    first = _run(tmp_path, runtime, table, version="eu")
    assert first.returncode == 0, first.stderr + first.stdout
    assert "extracting runtime from" in first.stdout
    assert (runtime / "SLES_039.36").read_bytes() == info["exe"]
    assert (runtime / "boot_tim.bin").read_bytes() == info["tim"]
    assert (runtime / "AAA" / "PRO" / "DEMO.PRO").read_bytes() == info["demo"]
    assert (runtime / "AAA" / "STR" / "CLIP.STR").read_bytes() == info["str_raw"]
    mapped = json.loads((runtime / "files_eu.json").read_text(encoding="utf-8"))
    assert mapped["version"] == "eu"
    assert mapped["executable"] == "SLES_039.36"
    assert mapped["files"]["1"]["path"] == "AAA/PRO/DEMO.PRO"
    assert not (runtime / "files_us.json").exists()
    assert (runtime / "memcard").is_dir()
    assert list((runtime / "memcard").iterdir()) == []

    manifest_mtime = (runtime / "files_eu.json").stat().st_mtime_ns
    second = _run(tmp_path, runtime, table, version="eu")
    assert second.returncode == 0, second.stderr + second.stdout
    assert "extracting runtime" not in second.stdout
    assert (runtime / "files_eu.json").stat().st_mtime_ns == manifest_mtime


def test_eu_rejects_us_disc_without_creating_runtime(tmp_path: Path):
    card = tmp_path / "sd" / "switch" / "dw3"
    card.mkdir(parents=True)
    info = _build_image(card / "disc.bin", version="us")
    table = tmp_path / "eu-table.txt"
    _write_table(table, _image_rows(info))

    result = _run(tmp_path, card / "eu", table, version="eu")
    assert result.returncode == 1
    assert "disc region mismatch: expected SLES_039.36, found SLUS_014.36" in result.stdout
    assert not (card / "eu").exists()


def test_eu_partial_extract_has_no_manifest_and_can_resume(tmp_path: Path):
    runtime = tmp_path / "sd" / "switch" / "dw3" / "eu"
    runtime.mkdir(parents=True)
    disc = runtime / "disc.bin"
    info = _build_image(disc, version="eu")
    complete_image = disc.read_bytes()
    damaged = bytearray(complete_image)
    damaged[info["str_lba"] * SECTOR] = 0x12
    disc.write_bytes(damaged)
    table = tmp_path / "eu-table.txt"
    _write_table(table, _image_rows(info))

    failed = _run(tmp_path, runtime, table, version="eu")
    assert failed.returncode == 1
    assert "extract write failed" in failed.stdout
    assert (runtime / "AAA" / "PRO" / "DEMO.PRO").read_bytes() == info["demo"]
    assert not (runtime / "files_eu.json").exists()
    assert not (runtime / "memcard").exists()

    disc.write_bytes(complete_image)
    resumed = _run(tmp_path, runtime, table, version="eu")
    assert resumed.returncode == 0, resumed.stderr + resumed.stdout
    assert (runtime / "files_eu.json").is_file()
    assert (runtime / "SLES_039.36").read_bytes() == info["exe"]


def test_corrupt_bin_fails_when_the_tree_is_missing(tmp_path: Path):
    card = tmp_path / "card"
    runtime = card / "us"
    runtime.mkdir(parents=True)
    # Large enough for the table extent, but not an ISO.
    (card / "game.bin").write_bytes(b"\x00" * (40 * SECTOR))
    table = tmp_path / "table.txt"
    _write_table(table, [(1, 30, 2)])
    result = _run(tmp_path, runtime, table)
    assert result.returncode == 1
    assert not (runtime / "files_us.json").exists()
