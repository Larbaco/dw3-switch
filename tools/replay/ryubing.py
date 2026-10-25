#!/usr/bin/env python3
"""P6.15 replay of the Switch build in Ryubing (driven by tools/replay_ryubing.sh).

Builds (or takes) the replay NRO (`tools/build_switch_game.sh --replay`),
copies the scenario's .rec to sdmc:/switch/dw3/replay.rec in a private Ryubing
data root, runs `Ryujinx -r <root> --use-main-config <nro>` on its own Xvfb and
reads the guest log (svcOutputDebugString lines in the Ryujinx output).

The verdict does not compare pictures. It checks:
  * no "Unhandled exception" / "Invalid memory access" / guest crash report;
  * every `replay shot <frame> <name>` line of the recording appears, in order;
  * `replay end` is reached before the emulator exits;
  * the overlay restore markers (`kind=mode|sub`) match the scenario's
    expected sequence and none is `kind=unknown`;
  * with an nxlink build, the `mem dbg:` high-water marks are flat after warm-up;
  * no game-frame progress for --stall seconds is a HANG.
Screenshots of the Xvfb screen at each checkpoint are kept as informational
evidence only (never compared).

Own data roots only (default /mnt/cabal/tmp/dw3-p615-ryu/{us,eu}); the user's
~/.config/Ryujinx/Config.json and other agents' roots are never touched.
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest  # noqa: E402
import replay_tests as rt  # noqa: E402

ROOT = rt.ROOT
RYUJINX = "/mnt/windows/Jogos/emuladores/Ryubing/build/Ryujinx"
DEFAULT_BASE = "/mnt/cabal/tmp/dw3-p615-ryu"
DEFAULT_SEED = "/mnt/cabal/tmp/dw3-preview29-ryu/us"   # keys/config/profiles to copy
DEFAULT_OUT = "/mnt/windows/Jogos/dw3-switch/tmp/evidence/p615-ryubing"

# Memory flatness (nxlink builds): `mem dbg:` high-water marks may rise by at
# most this many KiB between the warm-up point and the end of the run.
MEM_HW_GROWTH_KIB = 512
FATAL_RE = re.compile(r"Unhandled exception|Invalid memory access|Guest crash|"
                      r"ExecutionEngineException|StackOverflow|Segmentation fault")
SHOT_LINE = re.compile(r"replay shot (\d+) ([A-Za-z0-9_-]+)")
END_LINE = re.compile(r"replay end (\d+)")
FRAME_LINE = re.compile(r"dw3-replay: frame (\d+)")
MEM_LINE = re.compile(r"mem dbg: heap=(\d+)K/\d+K hw=(\d+)K .*?fcache=\d+K/hw=(\d+)K .*?libc=\d+K/hw=(\d+)K")
GUEST_RE = re.compile(r"OutputDebugString: (.*)")


def nro_path(version: str, nxlink: bool) -> Path:
    d = "switch" + ("-eu" if version == "eu" else "") + ("-nxlink" if nxlink else "") + "-replay"
    name = "dw3-switch" + ("-nxlink" if nxlink else "") + "-replay.nro"
    return ROOT / "build" / d / name


def build_nro(version: str, nxlink: bool) -> Path:
    cmd = [str(ROOT / "tools/build_switch_game.sh"), "--replay"]
    if nxlink:
        cmd.append("--nxlink")
    if version == "eu":
        cmd.append("--eu")
    subprocess.run(cmd, check=True, cwd=ROOT)
    p = nro_path(version, nxlink)
    if not p.is_file():
        raise SystemExit(f"build did not produce {p}")
    return p


def prepare_root(base: Path, version: str, seed: Path):
    """Create the private data root: copy keys/config from the seed root and
    the owner runtime (no memcard) from the runtime cache. Never writes to the
    seed or to ~/.config/Ryujinx."""
    root = base / version
    if root.resolve() == seed.resolve() or "preview29" in str(root):
        raise SystemExit(f"refusing to use {root} as own data root")
    root.mkdir(parents=True, exist_ok=True)
    for item in ("Config.json", "bis", "games", "system", "profiles"):
        src, dst = seed / item, root / item
        if dst.exists():
            continue
        if not src.exists():
            raise SystemExit(f"seed root {seed} lacks {item} (use --seed-root)")
        if src.is_dir():
            shutil.copytree(src, dst, symlinks=True)
        else:
            shutil.copy2(src, dst)
    sd = root / "sdcard/switch/dw3"
    rtdir = sd / version
    if not (rtdir / "boot_tim.bin").is_file():
        src = rt.find_runtime(version, None)
        if src is None:
            raise SystemExit(f"no owner runtime for {version} (docs/DISC_SETUP.md)")
        rtdir.mkdir(parents=True, exist_ok=True)
        subprocess.run(f"cd '{src}' && tar cf - --exclude=./memcard . | (cd '{rtdir}' && tar xf -)",
                       shell=True, check=True)
    (rtdir / "memcard").mkdir(exist_ok=True)
    return root


def phase_expectations(sc, phase_idx):
    """(shots, end_frame) of one phase recording: [(frame, name)]."""
    shots, end = [], 0
    for line in (rt.SCEN_DIR / f"{sc.phases[phase_idx]}.rec").read_text().splitlines():
        f = line.split("#", 1)[0].split()
        if len(f) == 3 and f[0] == "shot":
            shots.append((int(f[1]), f[2]))
        elif len(f) == 2 and f[0] == "end":
            end = int(f[1])
    return shots, end


def guest_lines(text: str):
    out = []
    for line in text.splitlines():
        m = GUEST_RE.search(line)
        out.append(m.group(1) if m else line)
    return out


def mem_verdict(lines, warm_frame_idx):
    """`mem dbg:` high-water flatness. Returns (ok, detail) or None if absent."""
    samples = [tuple(int(x) for x in m.groups()) for m in
               (MEM_LINE.search(l) for l in lines) if m]
    if len(samples) < 8:
        return None
    # skip the first 60%: opening, movie, first transitions
    cut = int(len(samples) * 0.6)
    base, last = samples[cut], samples[-1]
    names = ("heap", "heap hw", "fcache hw", "libc hw")
    growth = [last[i] - base[i] for i in (1, 2, 3)]
    ok = all(g <= MEM_HW_GROWTH_KIB for g in growth)
    detail = ("mem dbg hw growth since 60% mark: " +
              ", ".join(f"{n} {g:+d}K" for n, g in zip(names[1:], growth)) +
              f" (limit {MEM_HW_GROWTH_KIB}K, {len(samples)} samples)")
    return ok, detail


def run_phase(sc, idx, root, nro, display, outdir, ryujinx, stall, timeout, label, stamp):
    sd = root / "sdcard/switch/dw3"
    shots, end_frame = phase_expectations(sc, idx)
    shutil.copyfile(rt.SCEN_DIR / f"{sc.phases[idx]}.rec", sd / "replay.rec")
    for stale in list(sd.glob("crash_report*")) + [sd / "dw3.log"]:
        try:
            stale.unlink()
        except FileNotFoundError:
            pass
    outdir.mkdir(parents=True, exist_ok=True)
    log = outdir / f"{stamp}-{label}.out"
    xdg = Path(f"/mnt/cabal/tmp/dw3-p615-ryu/xdg-{os.getpid()}-{label}")
    xdg.mkdir(parents=True, exist_ok=True)
    os.chmod(xdg, 0o700)
    env = dict(os.environ, DISPLAY=display, XDG_RUNTIME_DIR=str(xdg),
               DOTNET_ROOT=os.environ.get("DOTNET_ROOT", str(Path.home() / ".dotnet")))
    t0 = time.time()
    last_progress, last_frame, seen_shots, ended, fatal = t0, 0, [], None, None
    shot_png = set()
    verdict, detail = "OK", ""
    with open(log, "wb") as lf:
        proc = subprocess.Popen([ryujinx, "-r", str(root), "--use-main-config", str(nro)],
                                env=env, stdout=lf, stderr=subprocess.STDOUT,
                                start_new_session=True)
        pos = 0
        while True:
            time.sleep(1.0)
            now = time.time()
            with open(log, "rb") as rf:
                rf.seek(pos)
                chunk = rf.read()
                pos += len(chunk)
            text = chunk.decode("utf-8", "replace")
            for l in text.splitlines():
                m = SHOT_LINE.search(l)
                if m and (m.group(1), m.group(2)) not in seen_shots:
                    seen_shots.append((m.group(1), m.group(2)))
                    last_progress = now
                    last_frame = max(last_frame, int(m.group(1)))
                    if m.group(2) not in shot_png:   # informational screenshot
                        shot_png.add(m.group(2))
                        subprocess.run(["import", "-display", display, "-window", "root",
                                        str(outdir / f"{label}-{m.group(2)}.png")],
                                       stderr=subprocess.DEVNULL, timeout=30)
                m = FRAME_LINE.search(l)
                if m and int(m.group(1)) > last_frame:
                    last_frame, last_progress = int(m.group(1)), now
                if END_LINE.search(l):
                    ended = now
                if fatal is None and FATAL_RE.search(l):
                    fatal = l.strip()[:200]
            if proc.poll() is not None:
                break
            if fatal:
                verdict, detail = "CRASH", fatal
            elif ended and now - ended > 5:
                break
            elif now - t0 > timeout:
                verdict, detail = "TIMEOUT", f"over {timeout}s (last frame {last_frame})"
            elif now - last_progress > stall:
                verdict, detail = "HANG", f"no game-frame progress for {stall}s (last frame {last_frame})"
            if verdict != "OK":
                break
        # stop the emulator (the guest exit(0) at `replay end` leaves it running)
        for sig in (signal.SIGTERM, signal.SIGKILL):
            try:
                os.killpg(proc.pid, sig)
            except ProcessLookupError:
                break
            try:
                proc.wait(10)
                break
            except subprocess.TimeoutExpired:
                continue
    full = log.read_text(errors="replace")
    lines = guest_lines(full)
    rows = []
    if verdict == "OK":
        if fatal is None:
            m = FATAL_RE.search(full)
            if m:
                verdict, detail = "CRASH", m.group(0)
        if verdict == "OK" and not ended:
            verdict, detail = "NOEND", f"emulator exited before `replay end` (last frame {last_frame})"
    crash_file = sd / "crash_report.txt"
    if verdict == "OK" and crash_file.is_file() and crash_file.stat().st_size > 0:
        verdict, detail = "CRASH", f"guest wrote {crash_file} ({crash_file.stat().st_size} bytes)"
    secs = time.time() - t0
    rows.append((sc.name, f"(process p{idx + 1})", "PASS" if verdict == "OK" else verdict,
                 f"{secs:.0f}s, last frame {last_frame}" if verdict == "OK" else detail))
    # shots in order
    want = [(str(f), n) for f, n in shots]
    got = [s for s in seen_shots]
    got_names = [n for _, n in got]
    ok_shots = got_names == [n for _, n in want] and all(
        int(g[0]) >= int(w[0]) for g, w in zip(got, want))
    if ok_shots:
        rows.append((sc.name, f"(shots p{idx + 1})", "PASS", f"{len(want)} `replay shot` lines in order"))
    else:
        missing = [n for _, n in want if n not in got_names]
        rows.append((sc.name, f"(shots p{idx + 1})", "FAIL",
                     f"expected {len(want)} in order, saw {len(got)}; missing {missing[:4]}"))
    for p in rt.check_log_expect(sc.expect_log, idx, "\n".join(lines), sc.forbid_log):
        rows.append((sc.name, f"(log p{idx + 1})", "FAIL", p))
    if any(i == idx for i, _ in list(sc.expect_log) + list(sc.forbid_log)) and not any(
            r[1] == f"(log p{idx + 1})" for r in rows):
        rows.append((sc.name, f"(log p{idx + 1})", "PASS", "expected log line(s) found"))
    probs = rt.check_restores("\n".join(lines), sc.expect_restore if idx == len(sc.phases) - 1 and len(sc.phases) == 1 else ())
    for p in probs:
        rows.append((sc.name, f"(markers p{idx + 1})", "FAIL", p))
    if not probs:
        rows.append((sc.name, f"(markers p{idx + 1})", "PASS",
                     f"{len(rt.parse_restores(chr(10).join(lines)))} overlay restores, none unknown"))
    mv = mem_verdict(lines, 0)
    if mv is not None:
        rows.append((sc.name, f"(mem dbg p{idx + 1})", "PASS" if mv[0] else "FAIL", mv[1]))
    return verdict, rows, log


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--version", choices=("us", "eu", "both"), default="both")
    ap.add_argument("--only", action="append", help="scenario name (repeatable)")
    ap.add_argument("--quick", action="store_true", help="title-menu scenarios only")
    ap.add_argument("--soak", action="store_true", help="include the soak scenarios")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--build", action="store_true",
                    help="build the replay NRO with tools/build_switch_game.sh --replay first")
    ap.add_argument("--no-nxlink", action="store_true",
                    help="plain --replay build (no `mem dbg:` samples, no debug HUD)")
    ap.add_argument("--nro-us")
    ap.add_argument("--nro-eu")
    ap.add_argument("--root-base", default=DEFAULT_BASE, help="own Ryubing data roots (<base>/{us,eu})")
    ap.add_argument("--seed-root", default=DEFAULT_SEED, help="read-only root to copy keys/config from")
    ap.add_argument("--ryujinx", default=RYUJINX)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--stall", type=int, default=600,
                    help="seconds without game-frame progress => HANG (default 600; the JIT is slow at boot)")
    ap.add_argument("--timeout", type=int, default=None, help="per phase seconds (default per scenario x6)")
    ap.add_argument("--scen-dir")
    a = ap.parse_args()
    if a.scen_dir:
        rt.SCEN_DIR = Path(a.scen_dir).expanduser().resolve()
    if a.list:
        for sc in manifest.SCENARIOS:
            print(sc.name, sc.tier)
        return 0
    names = {sc.name for sc in manifest.SCENARIOS}
    if a.only:
        unknown = sorted(set(a.only) - names)
        if unknown:
            ap.error(f"unknown scenario(s): {', '.join(unknown)}")
        sel = set(a.only)
    elif a.quick:
        sel = {sc.name for sc in manifest.SCENARIOS if sc.tier == "quick"}
    else:
        tiers = {"quick", "full"} | ({"soak"} if a.soak else set())
        sel = {sc.name for sc in manifest.SCENARIOS if sc.tier in tiers}
    versions = ("us", "eu") if a.version == "both" else (a.version,)
    todo = [sc for sc in manifest.SCENARIOS if sc.name in sel and sc.version in versions]
    for sc in [sc for sc in todo if sc.env]:
        # The NRO has no environment for the scenario's debug aids: PC only.
        print(f"skip {sc.name}: needs host env ({', '.join(k for k, _ in sc.env)}), PC only")
    todo = [sc for sc in todo if not sc.env]
    for sc in [sc for sc in todo if sc.quits]:
        # The guest ends the emulator session by itself: nothing to replay on.
        print(f"skip {sc.name}: the game quits by itself (P6.28), PC only")
    todo = [sc for sc in todo if not sc.quits]
    if not todo:
        ap.error("no scenarios selected")

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = Path(a.out).expanduser().resolve() / f"{stamp}-{a.version}"
    out.mkdir(parents=True, exist_ok=True)
    base = Path(a.root_base).expanduser().resolve()
    nxlink = not a.no_nxlink
    nros, roots = {}, {}
    for v in sorted({sc.version for sc in todo}):
        arg = getattr(a, f"nro_{v}")
        nros[v] = Path(arg).resolve() if arg else (build_nro(v, nxlink) if a.build else nro_path(v, nxlink))
        if not nros[v].is_file():
            ap.error(f"missing replay NRO {nros[v]} (use --build)")
        roots[v] = prepare_root(base, v, Path(a.seed_root))

    rows = []
    for sc in todo:
        root, nro = roots[sc.version], nros[sc.version]
        memdir = root / "sdcard/switch/dw3" / sc.version / "memcard"
        shutil.rmtree(memdir, ignore_errors=True)   # fresh card per scenario
        memdir.mkdir(parents=True)
        # P9.4: and fresh options (the phases of one scenario share the file)
        for stale in ("port_options.cfg", "port_options.cfg.tmp"):
            (memdir.parent / stale).unlink(missing_ok=True)
        if sc.cfg:  # P6.18: seed the pacing rows through the option file
            rt.write_seed_cfg(memdir.parent, sc.cfg)
        xvfb, display = rt.start_xvfb(out)
        try:
            for i in range(len(sc.phases)):
                label = f"{sc.name}-p{i + 1}" if len(sc.phases) > 1 else sc.name
                timeout = a.timeout or max(sc.timeout * 6, 1800)
                print(f"[{label}] running in Ryubing ({nro.name})", flush=True)
                v, r, log = run_phase(sc, i, root, nro, display, out / sc.name, a.ryujinx,
                                      a.stall, timeout, label, stamp)
                rows += r
                print(f"[{label}] {v} -> {log}", flush=True)
                if v != "OK":
                    break
        finally:
            xvfb.terminate()
            try:
                xvfb.wait(5)
            except subprocess.TimeoutExpired:
                xvfb.kill()
    w = max(len(r[0]) for r in rows)
    lines = [f"{'scenario':<{w}}  {'check':<22} result   detail"]
    for r in rows:
        lines.append(f"{r[0]:<{w}}  {r[1]:<22} {r[2]:<8} {r[3]}")
    bad = [r for r in rows if r[2] not in ("PASS", "INFO")]
    lines.append(f"\n{len(rows)} checks, {len(bad)} failed; evidence in {out}")
    text = "\n".join(lines)
    print(text)
    (out / "report.txt").write_text(text + "\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
