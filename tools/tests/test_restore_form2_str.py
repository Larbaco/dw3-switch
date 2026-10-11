import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from restore_form2_str import RAW_SECTOR, restore  # noqa: E402


def test_restore_writes_raw_mode2_and_keeps_xa_tail(tmp_path):
    runtime = tmp_path / "us"
    str_dir = runtime / "AAA" / "STR"
    str_dir.mkdir(parents=True)
    lba = 3
    sectors = 2
    # Byte past the 2048-byte extract, inside the Form2 XA tail.
    marker_at = 24 + 2100
    raw = bytearray(sectors * RAW_SECTOR)
    raw[0 * RAW_SECTOR + marker_at] = 0xA5
    raw[1 * RAW_SECTOR + marker_at] = 0x5A
    disc = tmp_path / "game.bin"
    # Image must cover LBA 3 and 4.
    blob = bytearray((lba + sectors) * RAW_SECTOR)
    blob[lba * RAW_SECTOR : (lba + sectors) * RAW_SECTOR] = raw
    disc.write_bytes(blob)

    dest = str_dir / "MOVIEOPN.STR"
    dest.write_bytes(b"\x00" * (sectors * 2048))
    file_map = {
        "version": "us",
        "files": {
            "1": {
                "path": "AAA/STR/MOVIEOPN.STR",
                "lba": lba,
                "sectors": sectors,
                "bytes": sectors * 2048,
                "form": 2,
            }
        },
    }
    (runtime / "files_us.json").write_text(json.dumps(file_map), encoding="utf-8")

    assert restore(runtime, disc) == 0
    got = dest.read_bytes()
    assert len(got) == sectors * RAW_SECTOR
    assert got[marker_at] == 0xA5
    assert got[RAW_SECTOR + marker_at] == 0x5A
    # Second run leaves the restored file in place.
    assert restore(runtime, disc) == 0
    assert dest.read_bytes() == got


def _pick(tmp_path: Path, root: Path, min_bytes: int):
    exe = tmp_path / "str_disc_pick"
    if not exe.exists():
        subprocess.check_call(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "port" / "movies"),
                "-o",
                str(exe),
                str(ROOT / "port" / "movies" / "str_disc.c"),
                str(ROOT / "tools" / "tests" / "str_disc_pick_main.c"),
            ]
        )
    env = os.environ.copy()
    env.pop("DW3_DISC_BIN", None)
    env.pop("DW3_DISC_CUE", None)
    result = subprocess.run(
        [str(exe), str(root), str(min_bytes)],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    return result.returncode, result.stdout.strip(), result.stderr


def test_skips_boot_tim_and_uses_parent_bin_when_cue_name_differs(tmp_path):
    parent = tmp_path / "dw3"
    runtime = parent / "us"
    runtime.mkdir(parents=True)
    (runtime / "boot_tim.bin").write_bytes(b"TIM" * 100)
    real = parent / "Digimon World 3.bin"
    real.write_bytes(b"\x00" * (RAW_SECTOR * 4))
    (parent / "Digimon World 3.cue").write_text(
        'FILE "Digimon World 3 (USA).bin" BINARY\n'
        "  TRACK 01 MODE2/2352\n"
        "    INDEX 01 00:00:00\n",
        encoding="utf-8",
    )
    code, out, err = _pick(tmp_path, runtime, RAW_SECTOR * 4)
    assert code == 0, err
    assert out == str(real)


def test_small_parent_bin_is_not_a_disc(tmp_path):
    parent = tmp_path / "dw3"
    runtime = parent / "us"
    runtime.mkdir(parents=True)
    (runtime / "boot_tim.bin").write_bytes(b"x" * 100)
    (parent / "Digimon World 3.bin").write_bytes(b"\x00" * 1000)
    (parent / "Digimon World 3.cue").write_text(
        'FILE "nope.bin" BINARY\n', encoding="utf-8"
    )
    code, _out, _err = _pick(tmp_path, runtime, RAW_SECTOR * 10)
    assert code == 1


def test_cue_file_name_wins_when_the_named_bin_exists(tmp_path):
    card = tmp_path / "card"
    card.mkdir()
    named = card / "exact.bin"
    named.write_bytes(b"\x00" * (RAW_SECTOR * 2))
    (card / "other.bin").write_bytes(b"\x00" * (RAW_SECTOR * 2))
    (card / "game.cue").write_text('FILE "exact.bin" BINARY\n', encoding="utf-8")
    code, out, err = _pick(tmp_path, card, RAW_SECTOR * 2)
    assert code == 0, err
    assert out == str(named)


def test_two_bins_and_a_wrong_cue_do_not_guess(tmp_path):
    card = tmp_path / "card"
    card.mkdir()
    (card / "a.bin").write_bytes(b"\x00" * (RAW_SECTOR * 2))
    (card / "b.bin").write_bytes(b"\x00" * (RAW_SECTOR * 2))
    (card / "game.cue").write_text('FILE "missing.bin" BINARY\n', encoding="utf-8")
    code, _out, _err = _pick(tmp_path, card, RAW_SECTOR * 2)
    assert code == 1
