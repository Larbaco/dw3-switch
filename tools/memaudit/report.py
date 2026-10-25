#!/usr/bin/env python3
"""P5.1 reports: tabulate and plot a soak CSV, and symbolize an allocation audit.

  report.py mem   mem.csv [--warmup-min 10] [--svg out.svg] [--title T]
  report.py audit PREFIX --bin dw3-pc [--top 25]

`mem` reads the CSV written by platforms/pc/src/mem_sampler.c (DW3_MEM_LOG)
and prints, per metric, the median of the first and last tenth after the
warm-up, the maximum, and the least-squares slope in KiB per hour; with --svg
it draws one small chart per metric (no plotting library needed).

`audit` reads PREFIX.{summary.txt,windows.tsv,sites.tsv,deltas.tsv} written by
tools/memaudit/malloc_audit.c and prints the allocation sites of the game
thread and of the other threads, symbolized with addr2line.
"""
import argparse
import csv
import statistics
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

METRICS = [
    ("heap_used", "game heap used (KiB)", 1024),
    ("heap_hw", "game heap high-water (KiB)", 1024),
    ("fcache", "file cache (KiB)", 1024),
    ("mode_bytes", "mode-tag heap (KiB)", 1024),
    ("libc_inuse", "glibc in use (KiB)", 1024),
    ("libc_mmap", "glibc mmapped (KiB)", 1024),
    ("libc_arena", "glibc arena (KiB)", 1024),
    ("rss_kb", "process RSS (KiB)", 1),
    ("hwm_kb", "RSS peak (KiB)", 1),
    ("vmdata_kb", "VmData (KiB)", 1),
    ("threads", "threads", 1),
    ("fds", "open fds", 1),
]


def load_csv(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            try:
                rows.append({k: float(v) for k, v in r.items()})
            except (TypeError, ValueError):
                continue  # a truncated last row
    return rows


def slope(xs, ys):
    n = len(xs)
    if n < 3:
        return 0.0
    mx, my = sum(xs) / n, sum(ys) / n
    den = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den if den else 0.0


def mem_report(a):
    rows = load_csv(a.csv)
    if not rows:
        sys.exit("no rows")
    t_end = rows[-1]["wall_s"]
    sel = [r for r in rows if r["wall_s"] >= a.warmup_min * 60]
    if len(sel) < 10:
        sel = rows
    tenth = max(1, len(sel) // 10)
    print(f"# {a.title or a.csv}")
    print(f"samples {len(rows)}, wall {t_end / 3600:.2f} h, frames {int(rows[-1]['frame'])}, "
          f"analysed from {sel[0]['wall_s'] / 60:.0f} min ({len(sel)} samples)\n")
    print("| metric | first 10% median | last 10% median | max | slope KiB/h | change |")
    print("|---|---|---|---|---|---|")
    for key, label, div in METRICS:
        if key not in sel[0]:
            continue
        ys = [r[key] / div * (1 if div == 1 else 1) for r in sel]
        if div == 1024:
            ys = [r[key] / 1024.0 for r in sel]
        xs = [r["wall_s"] / 3600.0 for r in sel]
        a0 = statistics.median(ys[:tenth])
        a1 = statistics.median(ys[-tenth:])
        sl = slope(xs, ys)
        print(f"| {label} | {a0:.0f} | {a1:.0f} | {max(ys):.0f} | {sl:+.0f} | {a1 - a0:+.0f} |")
    if a.svg:
        svg(rows, a)


def svg(rows, a):
    W, H, PAD = 760, 150, 46
    keys = [m for m in METRICS if m[0] in rows[0] and m[0] not in ("hwm_kb",)]
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{(H + 30) * len(keys) + 30}" '
           f'font-family="sans-serif" font-size="11">',
           f'<text x="10" y="18" font-size="14">{a.title or a.csv}</text>']
    xs = [r["wall_s"] for r in rows]
    x0, x1 = min(xs), max(xs) or 1
    for i, (key, label, div) in enumerate(keys):
        oy = 30 + i * (H + 30)
        ys = [r[key] / (1024.0 if div == 1024 else 1.0) for r in rows]
        lo, hi = min(ys), max(ys)
        if hi - lo < 1e-9:
            lo, hi = lo - 1, hi + 1
        pts = " ".join(f"{PAD + (x - x0) / (x1 - x0 or 1) * (W - PAD - 10):.1f},"
                       f"{oy + H - (y - lo) / (hi - lo) * H:.1f}" for x, y in zip(xs, ys))
        out.append(f'<rect x="{PAD}" y="{oy}" width="{W - PAD - 10}" height="{H}" fill="none" stroke="#999"/>')
        out.append(f'<polyline points="{pts}" fill="none" stroke="#1f6fb2" stroke-width="1.2"/>')
        out.append(f'<text x="{PAD}" y="{oy - 4}">{label}  (min {lo:.0f}, max {hi:.0f}; x = 0..{x1 / 3600:.2f} h)</text>')
        out.append(f'<text x="2" y="{oy + 10}">{hi:.0f}</text><text x="2" y="{oy + H}">{lo:.0f}</text>')
    out.append("</svg>")
    Path(a.svg).write_text("\n".join(out))
    print(f"\nchart: {a.svg}")


# --- audit ---------------------------------------------------------------

def symbolize(binary, addrs_by_mod):
    """{(mod, off): 'func file:line'} through addr2line (one call per module)."""
    res = {}
    for mod, offs in addrs_by_mod.items():
        path = binary if Path(mod).name == Path(binary).name else mod
        if not Path(path).exists():
            continue
        offs = sorted(offs)
        try:
            p = subprocess.run(["addr2line", "-f", "-C", "-p", "-e", path] + [hex(o) for o in offs],
                               capture_output=True, text=True, timeout=300)
        except Exception:
            continue
        lines = p.stdout.splitlines()
        if len(lines) == len(offs):
            for o, ln in zip(offs, lines):
                res[(mod, o)] = ln.replace(" at ", " ").split("/")[-1] if "/" in ln else ln
    return res


def parse_pc(tok):
    mod, _, off = tok.rpartition("+")
    try:
        return mod, int(off, 16)
    except ValueError:
        return mod, 0


SKIP = ("libc.so", "ld-linux", "libdw3_malloc_audit", "libgcc", "libstdc++")


def audit_report(a):
    pre = Path(a.prefix)
    summ = Path(f"{pre}.summary.txt")
    print(f"# allocation audit {pre.name}\n")
    if summ.exists():
        print("```\n" + summ.read_text() + "```\n")
    wins = Path(f"{pre}.windows.tsv")
    if wins.exists():
        rows = [ln.split("\t") for ln in wins.read_text().splitlines()[1:] if ln.strip()]
        if rows:
            tot = [sum(int(r[i]) for r in rows) for i in (2, 3, 4, 5, 6, 7)]
            print(f"windows {len(rows)}: main allocs {tot[0]} frees {tot[1]} bytes {tot[2]}; "
                  f"other allocs {tot[3]} frees {tot[4]} bytes {tot[5]}")
            live = [int(r[8]) for r in rows]
            print(f"tracked live (allocated after arming, not freed): first {live[0]}, "
                  f"last {live[-1]}, max {max(live)}\n")
    sites = []
    for ln in Path(f"{pre}.sites.tsv").read_text().splitlines()[1:]:
        f = ln.split("\t")
        if len(f) < 11:
            continue
        sites.append(dict(id=int(f[0]), main=int(f[1]), count=int(f[2]), bytes=int(f[3]),
                          frees=int(f[4]), live_n=int(f[5]), live_b=int(f[6]),
                          active=int(f[7]), first=int(f[8]), last=int(f[9]),
                          stack=[parse_pc(t) for t in f[10].split(",")]))
    need = defaultdict(set)
    for s in sites:
        for m, o in s["stack"]:
            need[m].add(o)
    sym = symbolize(a.bin, need)

    def name(m, o):
        return sym.get((m, o)) or f"{Path(m).name}+{o:#x}"

    exe = Path(a.bin).name

    def exe_frames(s):
        return [name(m, o).split(" ")[0] for m, o in s["stack"] if Path(m).name == exe]

    def label(s):
        """The frames inside the game binary (innermost first); a stack with
        none is a driver or SDL worker thread, labelled by its module."""
        fr = [f for f in exe_frames(s) if f != "??"]
        if fr:
            return " <- ".join(fr[:4])
        mods = [Path(m).name for m, o in s["stack"] if not any(k in m for k in SKIP)]
        return "[no game frame] " + (mods[0] if mods else "libc only")

    def klass(s):
        """Where the allocation comes from, by the innermost frames."""
        fr = exe_frames(s)
        joined = " ".join(fr)
        if not fr:
            return "driver/worker"
        if "VULKAN_" in joined or "SDL_GPU" in joined or "SDL_BindGPU" in joined or \
                "SDL_DrawGPU" in joined or "SDL_PushGPU" in joined or "SDL_Submit" in joined:
            return "SDL GPU (PC renderer)"
        if any(f.startswith("SDL_") or f.startswith("X11") or f.startswith("pulse") for f in fr):
            return "SDL other"
        if any(f.startswith(("Psyz_", "Draw_", "psyz", "Gpu", "GPU_")) for f in fr):
            return "psyz"
        if any(f.startswith("dw3_") or f.startswith("cdfs") or f.startswith("memcard") for f in fr):
            return "port"
        return "game/other"

    for main, title in ((1, "game (frame loop) thread"), (0, "other threads (audio, GPU driver, SDL)")):
        sel = sorted((s for s in sites if s["main"] == main), key=lambda s: -s["count"])
        print(f"\n## {title}: {len(sel)} sites, {sum(s['count'] for s in sel)} allocations, "
              f"{sum(s['live_n'] for s in sel)} still live\n")
        by = defaultdict(lambda: [0, 0, 0, 0])
        for s in sel:
            k = by[klass(s)]
            k[0] += 1
            k[1] += s["count"]
            k[2] += s["bytes"]
            k[3] += s["live_n"]
        print("| origin | sites | allocations | bytes | still live |")
        print("|---|---|---|---|---|")
        for k, v in sorted(by.items(), key=lambda kv: -kv[1][1]):
            print(f"| {k} | {v[0]} | {v[1]} | {v[2]} | {v[3]} |")
        print()
        print("| count | bytes | frames active | live now | first..last frame | origin | site |")
        print("|---|---|---|---|---|---|---|")
        shown = [s for s in sel if klass(s) not in a.hide.split(",")]
        for s in shown[:a.top]:
            print(f"| {s['count']} | {s['bytes']} | {s['active']} | {s['live_n']} ({s['live_b']} B) | "
                  f"{s['first']}..{s['last']} | {klass(s)} | `{label(s)}` |")
        leaks = sorted((s for s in sel if s["live_n"] > 2), key=lambda s: -s["live_b"])
        if leaks:
            print(f"\nSites with live allocations left at the end ({title}):\n")
            for s in leaks[:10]:
                print(f"- {s['live_n']} live / {s['count']} made, {s['live_b']} B: `{label(s)}`")

    if a.phases:
        phases_report(a, sites, klass, label, pre)


PHASE_NAMES = {0x158: "field", 0x166: "field", 0x159: "battle", 0x167: "battle",
               0x1C2: "battle report", 0x1D1: "battle report",
               0x1F9: "field menu (status)", 0x207: "field menu (status)",
               0x1E3: "save screen", 0x1EE: "save screen", 0x1C1: "title", 0x1CF: "title"}


def phases_report(a, sites, klass, label, pre):
    """Attribute each audit window to the game mode that was running: the last
    `overlay restore file=0x.. kind=mode` before the window's end, from the
    soak log, whose `dw3-mem: ... frame=N` lines give the frame of each point."""
    import re
    marks, cur = [], "boot"
    for ln in Path(a.phases).read_text(errors="replace").splitlines():
        m = re.search(r"overlay restore file=0x([0-9a-f]+) kind=mode", ln)
        if m:
            fid = int(m.group(1), 16)
            cur = PHASE_NAMES.get(fid, f"mode file 0x{fid:x}")
            continue
        m = re.search(r"dw3-mem: t=\S+ frame=(\d+)", ln)
        if m:
            marks.append((int(m.group(1)), cur))

    def phase_at(frame):
        best = "boot"
        for f, ph in marks:
            if f <= frame:
                best = ph
            else:
                break
        return best

    cls = {s["id"]: klass(s) for s in sites}
    lab = {s["id"]: label(s) for s in sites}
    wins = [ln.split("\t") for ln in Path(f"{pre}.windows.tsv").read_text().splitlines()[1:] if ln.strip()]
    per = defaultdict(lambda: dict(frames=0, main=0, other=0, nongpu=0, sites=defaultdict(int)))
    for w in wins:
        f0, f1 = int(w[0]), int(w[1])
        d = per[phase_at(f1)]
        d["frames"] += f1 - f0
        d["main"] += int(w[2])
        d["other"] += int(w[5])
    for ln in Path(f"{pre}.deltas.tsv").read_text().splitlines()[1:]:
        f = ln.split("\t")
        if len(f) < 5 or int(f[2]) != 1:
            continue
        sid, cnt = int(f[1]), int(f[3])
        if cls.get(sid) in ("SDL GPU (PC renderer)", "driver/worker"):
            continue
        ph = phase_at(int(f[0]))
        per[ph]["nongpu"] += cnt
        per[ph]["sites"][lab.get(sid, "?")] += cnt
    print("\n## Steady-state allocations by game mode (game thread)\n")
    print("| mode | frames | all allocs/frame | allocs/frame without the SDL GPU renderer | top remaining sites (allocs) |")
    print("|---|---|---|---|---|")
    for ph, d in sorted(per.items(), key=lambda kv: -kv[1]["frames"]):
        if d["frames"] == 0:
            continue
        top = sorted(d["sites"].items(), key=lambda kv: -kv[1])[:3]
        print(f"| {ph} | {d['frames']} | {d['main'] / d['frames']:.1f} | "
              f"{d['nongpu'] / d['frames']:.4f} | "
              + "; ".join(f"`{n}` {c}" for n, c in top) + " |")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("mem")
    m.add_argument("csv")
    m.add_argument("--warmup-min", type=float, default=10)
    m.add_argument("--svg")
    m.add_argument("--title")
    au = sub.add_parser("audit")
    au.add_argument("prefix")
    au.add_argument("--bin", required=True)
    au.add_argument("--top", type=int, default=25)
    au.add_argument("--phases", help="the soak log: attribute windows to game modes")
    au.add_argument("--hide", default="", help="comma list of origins to leave out of the site table")
    a = ap.parse_args()
    mem_report(a) if a.cmd == "mem" else audit_report(a)


if __name__ == "__main__":
    main()
