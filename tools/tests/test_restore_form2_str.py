import json
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
