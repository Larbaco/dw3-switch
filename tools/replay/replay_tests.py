#!/usr/bin/env python3
"""P6.15 regression replay runner (driven by tools/replay_tests.sh).

Replays the pad recordings in tools/replay/scenarios/ against the PC host build
under Xvfb, one scenario per process with a fresh memory-card dir, collects the
checkpoint screenshots the port writes (DW3_REPLAY_OUT) and compares them to
reference captures kept OUTSIDE git. Exit status 0 = every checkpoint matched.

Only PIL is needed (python3-pil). Xvfb and ImageMagick `import` must be on PATH.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
SCEN_DIR = Path(__file__).resolve().parent / "scenarios"

# Per-channel tolerance (0-255) below which two pixels count as equal, and the
# fraction of differing pixels a checkpoint may have. Same-machine runs are
# byte-identical (see docs/P6.15_REPLAY_TESTS.md); the slack exists for another
# GPU driver / Mesa version, which shifts a few edge pixels by a few levels.
# 8/255 ignores that rounding noise; 0.1% of the 1280x960 window (~1230 px)
# is far smaller than any real UI change (one menu label is ~5000 px) yet
# above resampling noise.
CHANNEL_TOL = 8
MAX_DIFF_RATIO = 0.001

SCENARIOS = [  # (name, version)
    ("us-title", "us"),
    ("eu-lang-en", "eu"),
    ("eu-lang-fr", "eu"),
    ("eu-lang-de", "eu"),
    ("eu-lang-it", "eu"),
    ("eu-lang-es", "eu"),
]
ARTIFACTS = ("png", "ppm")  # window grab (with host overlays), PS1 display frame


def compare_images(ref: Path, got: Path, diff_out: Path | None):
    """Return (differing_pixels, total_pixels, error). Writes a diff image."""
    a = Image.open(ref).convert("RGB")
    b = Image.open(got).convert("RGB")
    if a.size != b.size:
        return None, None, f"size {a.size} vs {b.size}"
    d = ImageChops.difference(a, b)
    # Collapse R,G,B: a pixel differs if any channel is over tolerance.
    r, g, bl = d.split()
    m = ImageChops.lighter(ImageChops.lighter(
        r.point(lambda v: 255 if v > CHANNEL_TOL else 0),
        g.point(lambda v: 255 if v > CHANNEL_TOL else 0)),
        bl.point(lambda v: 255 if v > CHANNEL_TOL else 0))
    n = m.histogram()[255]
    total = a.size[0] * a.size[1]
    if n and diff_out is not None:
        # Left: reference, middle: capture, right: diff (red where it differs).
        w, h = a.size
        out = Image.new("RGB", (w * 3, h))
        out.paste(a, (0, 0))
        out.paste(b, (w, 0))
        hl = Image.new("RGB", (w, h), (0, 0, 0))
        hl.paste(Image.new("RGB", (w, h), (255, 0, 0)), mask=m)
        out.paste(hl, (w * 2, 0))
        out.save(diff_out)
    return n, total, None


def find_runtime(version: str, arg: str | None) -> Path | None:
    cands = []
    if arg:
        cands.append(Path(arg))
    env = os.environ.get(f"DW3_REPLAY_RUNTIME_{version.upper()}")
    if env:
        cands.append(Path(env))
    cands += [ROOT / "runtime" / version,
              Path.home() / ".cache/dw3-switch/runtime" / version]
    for c in cands:
        if (c / "boot_tim.bin").is_file():
            return c
    return None


def build_host(version: str, work: Path) -> Path:
    bdir = work / f"build-{version}"
    base = ["cmake", "-S", str(ROOT / "platforms/pc"), "-B", str(bdir), "-G", "Ninja",
            f"-DDW3_VERSION={version}", "-DPSYZ_RENDERER=sdl3_gpu",
            "-DSDL_X11_XSCRNSAVER=OFF", "-DSDL_ALSA=OFF", "-DSDL_JACK=OFF",
            "-DSDL_PIPEWIRE=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_SNDIO=OFF"]
    subprocess.run(base, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["ninja", "-C", str(bdir), "dw3-pc"], check=True)
    return bdir / "dw3-pc"


def start_xvfb(work: Path):
    """Own Xvfb on a server-chosen free display (never an existing one)."""
    r, w = os.pipe()
    p = subprocess.Popen(
        ["Xvfb", "-displayfd", str(w), "-screen", "0", "1280x960x24", "-nolisten", "tcp"],
        pass_fds=(w,), stdout=subprocess.DEVNULL,
        stderr=open(work / "xvfb.log", "w"))
    os.close(w)
    buf = b""
    while not buf.endswith(b"\n"):  # Xvfb writes the number and "\n" separately
        chunk = os.read(r, 32)
        if not chunk:
            break
        buf += chunk
    disp = buf.decode().strip()
    os.close(r)
    if not disp:
        p.kill()
        raise RuntimeError("Xvfb did not report a display")
    return p, ":" + disp


def run_scenario(name, version, binary, runtime, display, outdir: Path, stamp, timeout):
    outdir.mkdir(parents=True, exist_ok=True)
    memdir = Path(tempfile.mkdtemp(prefix="dw3-replay-mc-"))
    xdg = Path(tempfile.mkdtemp(prefix="dw3-replay-xdg-"))
    os.chmod(xdg, 0o700)
    env = dict(os.environ, DISPLAY=display, XDG_RUNTIME_DIR=str(xdg),
               SDL_AUDIODRIVER="dummy", DW3_RUNTIME=str(runtime),
               DW3_MEMCARD_DIR=str(memdir),
               DW3_REPLAY=str(SCEN_DIR / f"{name}.rec"),
               DW3_REPLAY_OUT=str(outdir), DW3_REPLAY_XSHOT="1")
    for k in ("DW3_AUTO_PAD", "DW3_REPLAY_RECORD", "DW3_PC_INTERNAL_RES"):
        env.pop(k, None)
    log = outdir / f"{stamp}-{name}.log"  # rule 9: timestamped logs kept
    t0 = time.time()
    try:
        with open(log, "w") as lf:
            rc = subprocess.run([str(binary)], env=env, stdout=lf, stderr=subprocess.STDOUT,
                                timeout=timeout, cwd=outdir).returncode
    except subprocess.TimeoutExpired:
        rc = "timeout"
    shutil.rmtree(memdir, ignore_errors=True)
    shutil.rmtree(xdg, ignore_errors=True)
    return rc, time.time() - t0


def shot_names(name):
    return [l.split()[2] for l in (SCEN_DIR / f"{name}.rec").read_text().splitlines()
            if l.startswith("shot ")]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--update", action="store_true", help="(re)generate references from this run")
    ap.add_argument("--only", action="append", help="scenario name (repeatable)")
    ap.add_argument("--refs", default=os.environ.get(
        "DW3_REPLAY_REFS", str(Path.home() / ".cache/dw3-switch/replay-refs")),
        help="reference dir, OUTSIDE git (default ~/.cache/dw3-switch/replay-refs)")
    ap.add_argument("--out", default=os.environ.get("DW3_REPLAY_RESULTS", str(ROOT / "tmp/replay")),
                    help="results dir (captures, diffs, logs)")
    ap.add_argument("--bin-us", default=os.environ.get("DW3_REPLAY_BIN_US"))
    ap.add_argument("--bin-eu", default=os.environ.get("DW3_REPLAY_BIN_EU"))
    ap.add_argument("--runtime-us")
    ap.add_argument("--runtime-eu")
    ap.add_argument("--build", action="store_true", help="configure+build dw3-pc (us, eu) under --out")
    ap.add_argument("--scen-dir", help="recordings dir (default tools/replay/scenarios)")
    ap.add_argument("--timeout", type=int, default=180)
    a = ap.parse_args()

    global SCEN_DIR
    if a.scen_dir:
        SCEN_DIR = Path(a.scen_dir)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = Path(a.out) / stamp
    refs = Path(a.refs)
    out.mkdir(parents=True, exist_ok=True)
    refs.mkdir(parents=True, exist_ok=True)

    bins, runtimes = {}, {}
    for v in ("us", "eu"):
        arg = getattr(a, f"bin_{v}")
        if a.build and not arg:
            arg = str(build_host(v, out))
        if not arg:
            for c in (ROOT / f"build/pc{'' if v == 'us' else '-eu'}/dw3-pc",):
                if c.is_file():
                    arg = str(c)
        bins[v] = Path(arg) if arg else None
        runtimes[v] = find_runtime(v, getattr(a, f"runtime_{v}"))

    xvfb, display = start_xvfb(out)
    rows = []
    try:
        for name, ver in SCENARIOS:
            if a.only and name not in a.only:
                continue
            if not bins[ver] or not bins[ver].is_file() or runtimes[ver] is None:
                rows.append((name, "-", "SKIP", f"missing {'binary' if not bins[ver] else 'runtime'} for {ver}"))
                continue
            sdir = out / name
            rc, secs = run_scenario(name, ver, bins[ver], runtimes[ver], display, sdir, stamp, a.timeout)
            for shot in shot_names(name):
                for ext in ARTIFACTS:
                    got = sdir / f"{shot}.{ext}"
                    ref = refs / name / f"{shot}.{ext}"
                    label = f"{shot}.{ext}"
                    if not got.is_file():
                        rows.append((name, label, "FAIL", f"no capture (rc={rc})"))
                        continue
                    if a.update:
                        ref.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(got, ref)
                        rows.append((name, label, "REF", "reference written"))
                        continue
                    if not ref.is_file():
                        rows.append((name, label, "FAIL", "no reference (run with --update)"))
                        continue
                    diff = sdir / f"{shot}.{ext}.diff.png"
                    n, total, err = compare_images(ref, got, diff)
                    if err:
                        rows.append((name, label, "FAIL", err))
                    elif n / total > MAX_DIFF_RATIO:
                        rows.append((name, label, "FAIL", f"{n} px differ ({100*n/total:.3f}%) -> {diff}"))
                    else:
                        rows.append((name, label, "PASS", f"{n} px differ"))
            if rc != 0:
                rows.append((name, "(process)", "FAIL", f"exit {rc}"))
            print(f"[{name}] done in {secs:.1f}s rc={rc}", flush=True)
    finally:
        xvfb.terminate()
        try:
            xvfb.wait(5)
        except subprocess.TimeoutExpired:
            xvfb.kill()

    w = max(len(r[0]) for r in rows) if rows else 8
    lines = [f"{'scenario':<{w}}  {'checkpoint':<20} result  detail"]
    for r in rows:
        lines.append(f"{r[0]:<{w}}  {r[1]:<20} {r[2]:<6}  {r[3]}")
    bad = [r for r in rows if r[2] in ("FAIL", "SKIP")]
    lines.append(f"\n{len(rows)} checks, {len(bad)} failed/skipped; results in {out}")
    text = "\n".join(lines)
    print(text)
    (out / "report.txt").write_text(text + "\n")
    (out / "report.json").write_text(json.dumps(rows, indent=1))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
