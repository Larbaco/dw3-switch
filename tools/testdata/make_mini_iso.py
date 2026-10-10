#!/usr/bin/env python3
"""Generate tools/testdata/mini_iso.bin — synthetic MODE2/2352 ISO9660 fixture.

Not game data. Deterministic. Run from repo root:
  python3 tools/testdata/make_mini_iso.py
"""

from __future__ import annotations

import struct
from pathlib import Path

SECTOR = 2352
USER_DATA = 24
LOGICAL = 2048

ROOT_DIR_LBA = 19
PATH_TABLE_LBA = 18
DUMMY_LBA = 22
VOLUME_SECTORS = 24


def both_endian_u32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def both_endian_u16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def mode2_sector(user: bytes) -> bytes:
    if len(user) > LOGICAL:
        raise ValueError("user data too large")
    user = user.ljust(LOGICAL, b"\x00")
    sync = b"\x00" + b"\xff" * 10 + b"\x00"
    header = b"\x00\x00\x00\x02"  # MSF placeholder + Mode 2
    # Form 1 data subheader (duplicated): submode DATA|FORM1
    sub = bytes([0, 0, 0x08, 0, 0, 0, 0x08, 0])
    edc_ecc = b"\x00" * (SECTOR - USER_DATA - LOGICAL)
    return sync + header + sub + user + edc_ecc


def dir_record(
    name: bytes,
    lba: int,
    size: int,
    *,
    is_dir: bool = False,
) -> bytes:
    flags = 0x02 if is_dir else 0x00
    name_len = len(name)
    # date/time: 7 zero bytes is acceptable for a fixture
    body = bytearray()
    body.append(0)  # length placeholder
    body.append(0)  # XA length
    body += both_endian_u32(lba)
    body += both_endian_u32(size)
    body += b"\x00" * 7
    body.append(flags)
    body.append(0)  # file unit size
    body.append(0)  # interleave
    body += both_endian_u16(1)  # volume sequence
    body.append(name_len)
    body += name
    # ECMA-119: padding byte present only when File Identifier length is even.
    if (name_len & 1) == 0:
        body.append(0)
    body[0] = len(body)
    return bytes(body)


def build_path_table() -> bytes:
    # Root only: name_len=1, name=0x00, parent=1
    name = b"\x00"
    entry = bytearray()
    entry.append(len(name))
    entry.append(0)
    entry += struct.pack("<I", ROOT_DIR_LBA)
    entry += struct.pack("<H", 1)
    entry += name
    # Path table: pad when name_len is odd (ECMA-119 §9.4).
    if len(name) & 1:
        entry.append(0)
    return bytes(entry)


def build_root_directory() -> bytes:
    # Size of directory extent must cover all records.
    dummy_name = b"DUMMY.DAT;1"
    rec_dot = dir_record(b"\x00", ROOT_DIR_LBA, LOGICAL, is_dir=True)
    rec_dotdot = dir_record(b"\x01", ROOT_DIR_LBA, LOGICAL, is_dir=True)
    rec_file = dir_record(dummy_name, DUMMY_LBA, LOGICAL, is_dir=False)
    data = rec_dot + rec_dotdot + rec_file
    # Patch "." and ".." data lengths to actual directory size.
    size = len(data)
    rec_dot = dir_record(b"\x00", ROOT_DIR_LBA, size, is_dir=True)
    rec_dotdot = dir_record(b"\x01", ROOT_DIR_LBA, size, is_dir=True)
    return rec_dot + rec_dotdot + rec_file


def build_pvd(path_table: bytes, root_dir: bytes) -> bytes:
    pvd = bytearray(LOGICAL)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[40:72] = b"MINI_ISO".ljust(32, b" ")
    pvd[80:88] = both_endian_u32(VOLUME_SECTORS)
    pvd[120:124] = both_endian_u16(LOGICAL)
    pt_size = len(path_table)
    pvd[132:136] = struct.pack("<I", pt_size)
    pvd[136:140] = struct.pack(">I", pt_size)
    pvd[140:144] = struct.pack("<I", PATH_TABLE_LBA)
    pvd[148:152] = struct.pack(">I", PATH_TABLE_LBA)
    # Root directory record embedded in PVD (34 bytes at 156).
    root_rec = dir_record(b"\x00", ROOT_DIR_LBA, len(root_dir), is_dir=True)
    if len(root_rec) != 34:
        raise RuntimeError(f"root record must be 34 bytes, got {len(root_rec)}")
    pvd[156:190] = root_rec
    return bytes(pvd)


def build_terminator() -> bytes:
    term = bytearray(LOGICAL)
    term[0] = 255
    term[1:6] = b"CD001"
    term[6] = 1
    return bytes(term)


def main() -> None:
    out = Path(__file__).resolve().parent / "mini_iso.bin"
    path_table = build_path_table()
    root_dir = build_root_directory()
    pvd = build_pvd(path_table, root_dir)
    terminator = build_terminator()
    dummy = b"DUMMY" + b"\x00" * (LOGICAL - 5)

    sectors = [mode2_sector(b"\x00" * LOGICAL) for _ in range(VOLUME_SECTORS)]
    sectors[16] = mode2_sector(pvd)
    sectors[17] = mode2_sector(terminator)
    sectors[PATH_TABLE_LBA] = mode2_sector(path_table)
    sectors[ROOT_DIR_LBA] = mode2_sector(root_dir)
    sectors[DUMMY_LBA] = mode2_sector(dummy)

    data = b"".join(sectors)
    out.write_bytes(data)
    print(f"wrote {out} ({len(data)} bytes, {VOLUME_SECTORS} sectors)")


if __name__ == "__main__":
    main()
