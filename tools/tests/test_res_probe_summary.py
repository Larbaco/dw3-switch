"""P6.11 phase 0: tools/res_probe_summary.py reads the `dw3-res-probe:` lines."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "res_probe_summary.py"


def line(mode: str, res: int, fps: float, mesh_gpu: int, pres_gpu: int,
         scene: str = "battle", speed: int = 1) -> str:
    return (
        f"[INFO] nx_res_probe.c:1: dw3-res-probe: v=1 mode={mode} out=1280x720 "
        f"res={res} src=query speed={speed}x skip=0 scene={scene} game_fps={fps:.1f} "
        f"frames=60 shown=60 mesh_n=60 mesh_tris=78000 mesh_cpu_us=310 "
        f"mesh_cpu_max_us=900 mesh_up_us=20 mesh_gpu_us={mesh_gpu} "
        f"mesh_gpu_max_us={mesh_gpu * 2} mesh_gpu_n=60 pres_cpu_us=400 "
        f"pres_cpu_max_us=800 pres_gpu_us={pres_gpu} "
        f"pres_gpu_max_us={pres_gpu * 2} raster_us=1500 raster_max_us=2500 "
        f"swap_us=9000 skip_n=0 skip_us=0 busy_pct=45 q_drop=0 disjoint=0"
    )


def run(tmp_path: Path, lines: list[str], *extra: str):
    log = tmp_path / "dw3.log"
    log.write_text("\n".join(lines) + "\n")
    return subprocess.run([sys.executable, "-I", str(TOOL), str(log), *extra],
                          capture_output=True, text=True)


def test_clamp_suggestion(tmp_path: Path):
    lines = []
    # handheld: 1x and 2x keep 60 fps, 3x does not.
    for _ in range(8):
        lines.append(line("handheld", 1, 60.0, 400, 300))
    for _ in range(8):
        lines.append(line("handheld", 2, 60.0, 2500, 500))
    for _ in range(8):
        lines.append(line("handheld", 3, 52.0, 9000, 900))
    # docked: all three fine.
    for r in (1, 2, 3):
        for _ in range(8):
            lines.append(line("docked", r, 60.0, 300 * r, 200))
    r = run(tmp_path, lines)
    assert r.returncode == 0, r.stderr + r.stdout
    out = r.stdout
    hand = [ln for ln in out.splitlines() if ln.strip().startswith("handheld ")
            and " max " in ln]
    dock = [ln for ln in out.splitlines() if ln.strip().startswith("docked ")
            and " max " in ln]
    assert hand and "max 2x" in hand[0], out
    assert dock and "max 3x" in dock[0], out


def test_first_window_of_a_key_is_dropped(tmp_path: Path):
    # One window per key: the first of each key is partial and dropped, so the
    # tool has nothing to report.
    r = run(tmp_path, [line("handheld", 1, 60.0, 400, 300)])
    assert r.returncode == 1
    r = run(tmp_path, [line("handheld", 1, 60.0, 400, 300)], "--keep-first")
    assert r.returncode == 0


def test_ignores_other_lines(tmp_path: Path):
    r = run(tmp_path, ["pace dbg: speed=1x game_fps=60.0", "nonsense"])
    assert r.returncode == 1
