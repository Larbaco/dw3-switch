#!/usr/bin/env python3
"""Summarise the `dw3-res-probe:` lines of one or more dw3.log files (P6.11 phase 0).

The measurement build (docs/P6.11_INTERNAL_RES.md, "OLED rows R0.x") logs one
`dw3-res-probe: v=1 mode=... res=N ...` line per 60 game frames. This groups
them by (mode, res, scene, speed) and prints, for each group, the figures the
clamp table needs, then a suggested maximum scale per mode.

    tools/res_probe_summary.py dw3.log [dw3.1.log ...]
    tools/res_probe_summary.py --budget-us 12000 --min-fps 59.5 dw3.log

Windows that follow a change of scale or mode (the first window of a run of
one key) are dropped as partial, unless --keep-first.

A scale is allowed for a mode when, over its battle windows at speed 1x:
  * game_fps stays at or above --min-fps in at least --ok-pct percent of the
    windows (default 59.5 and 95), and
  * the worst window's mesh_gpu_us average plus pres_gpu_us average stays at or
    under --budget-us (default 12000 us of the 16667 us frame; the rest is for
    the CPU frame that runs in parallel and for the swap), and
  * at least --min-windows windows exist (default 5), else it is "unmeasured".
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

LINE = re.compile(r"dw3-res-probe: v=1 (?P<kv>.*)$")
PAIR = re.compile(r"(\w+)=(\S+)")

NUMERIC = (
    "frames shown mesh_n mesh_tris mesh_cpu_us mesh_cpu_max_us mesh_up_us "
    "mesh_gpu_us mesh_gpu_max_us mesh_gpu_n pres_cpu_us pres_cpu_max_us "
    "pres_gpu_us pres_gpu_max_us raster_us raster_max_us swap_us skip_n "
    "skip_us busy_pct q_drop disjoint res"
).split()


def parse_line(text: str) -> dict | None:
    m = LINE.search(text)
    if not m:
        return None
    rec: dict = {}
    for k, v in PAIR.findall(m.group("kv")):
        rec[k] = v
    try:
        for k in NUMERIC:
            if k in rec:
                rec[k] = int(rec[k])
        rec["game_fps"] = float(rec.get("game_fps", "0"))
        rec["speed"] = int(rec.get("speed", "1x").rstrip("x"))
        rec["skip"] = int(rec.get("skip", "0"))
    except ValueError:
        return None
    return rec


def load(paths: list[Path], keep_first: bool) -> list[dict]:
    out: list[dict] = []
    for p in paths:
        prev_key = None
        for line in p.read_text(errors="replace").splitlines():
            rec = parse_line(line)
            if rec is None:
                continue
            rec["_file"] = p.name
            key = (rec.get("mode"), rec.get("res"))
            first = key != prev_key
            prev_key = key
            if first and not keep_first:
                continue
            out.append(rec)
    return out


def pct(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    s = sorted(values)
    i = min(len(s) - 1, max(0, int(round(p / 100.0 * (len(s) - 1)))))
    return s[i]


def group(recs: list[dict]) -> dict:
    g: dict = defaultdict(list)
    for r in recs:
        g[(r["mode"], r["res"], r["scene"], r["speed"], r["skip"])].append(r)
    return g


def verdict(windows: list[dict], a: argparse.Namespace) -> tuple[str, str]:
    if len(windows) < a.min_windows:
        return "unmeasured", f"{len(windows)} windows"
    good = sum(1 for w in windows if w["game_fps"] >= a.min_fps)
    fps_pct = 100.0 * good / len(windows)
    worst = max(w["mesh_gpu_us"] + w["pres_gpu_us"] for w in windows)
    ok = fps_pct >= a.ok_pct and worst <= a.budget_us
    why = f"fps>={a.min_fps}: {fps_pct:.0f}%, worst gpu {worst} us"
    return ("ok" if ok else "NO"), why


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("logs", nargs="+", type=Path)
    ap.add_argument("--budget-us", type=int, default=12000)
    ap.add_argument("--min-fps", type=float, default=59.5)
    ap.add_argument("--ok-pct", type=float, default=95.0)
    ap.add_argument("--min-windows", type=int, default=5)
    ap.add_argument("--keep-first", action="store_true")
    a = ap.parse_args(argv)

    recs = load(a.logs, a.keep_first)
    if not recs:
        print("no `dw3-res-probe: v=1` lines found", file=sys.stderr)
        return 1
    groups = group(recs)
    print(f"{'mode':9} {'res':>3} {'scene':6} {'spd':>3} {'skp':>3} {'win':>4} "
          f"{'fps min':>8} {'fps med':>8} {'meshGPU med/p95/max us':>24} "
          f"{'presGPU med/max us':>19} {'meshCPU med':>11} {'raster med/max us':>18} "
          f"{'tris/win med':>12}")
    for key in sorted(groups, key=lambda k: (k[0], k[2], k[3], k[4], k[1])):
        w = groups[key]
        mode, res, scene, speed, skip = key
        fps = [x["game_fps"] for x in w]
        mg = [x["mesh_gpu_us"] for x in w]
        pg = [x["pres_gpu_us"] for x in w]
        print(f"{mode:9} {res:>3} {scene:6} {speed:>3} {skip:>3} {len(w):>4} "
              f"{min(fps):>8.1f} {statistics.median(fps):>8.1f} "
              f"{int(statistics.median(mg)):>7}/{int(pct(mg, 95)):>7}/"
              f"{max(x['mesh_gpu_max_us'] for x in w):>7} "
              f"{int(statistics.median(pg)):>9}/"
              f"{max(x['pres_gpu_max_us'] for x in w):>9} "
              f"{int(statistics.median(x['mesh_cpu_us'] for x in w)):>11} "
              f"{int(statistics.median(x['raster_us'] for x in w)):>8}/"
              f"{max(x['raster_max_us'] for x in w):>9} "
              f"{int(statistics.median(x['mesh_tris'] for x in w)):>12}")
    print()
    print("clamp suggestion (battle windows at speed 1x):")
    for mode in sorted({k[0] for k in groups}):
        allowed = 1
        parts = []
        for res in (1, 2, 3):
            w = groups.get((mode, res, "battle", 1, 0))
            if not w:
                parts.append(f"{res}x: no data")
                continue
            v, why = verdict(w, a)
            parts.append(f"{res}x: {v} ({why})")
            if v == "ok":
                allowed = res
        print(f"  {mode:9} max {allowed}x   " + "; ".join(parts))
    print()
    print("turbo (battle windows at speed > 1): game fps is the GPU's throughput")
    for key in sorted(groups):
        if key[2] == "battle" and key[3] > 1:
            w = groups[key]
            print(f"  {key[0]:9} res {key[1]} speed {key[3]}x skip {key[4]}: "
                  f"median game_fps {statistics.median(x['game_fps'] for x in w):.1f}"
                  f" over {len(w)} windows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
