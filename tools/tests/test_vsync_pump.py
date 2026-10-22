"""The Switch vsync pump advances its frame counter once per VSync.

`boot_main.c` needs libnx and cannot be built on the host, so this checks the
source: preview.29 advanced `pumps` twice per call, which halved every
"60-pump" window and doubled the nxlink HUD fps.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOOT_MAIN = ROOT / "platforms" / "switch" / "source" / "boot_main.c"


def _pump_body() -> str:
    src = BOOT_MAIN.read_text()
    start = src.index("static void dw3_nx_vsync_pump(void) {")
    depth = 0
    for i in range(src.index("{", start), len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start : i + 1]
    raise AssertionError("unterminated dw3_nx_vsync_pump")


def test_pump_counter_advances_once_per_call():
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", _pump_body(), flags=re.S)
    assert len(re.findall(r"\bpumps\s*\+\+|\+\+\s*pumps\b|\bpumps\s*\+=", code)) == 1
