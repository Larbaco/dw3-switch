#!/usr/bin/env python3
"""P6.15 regression replay runner (driven by tools/replay_tests.sh).

Replays the pad recordings in tools/replay/scenarios/ against the PC host build
under Xvfb, collects the checkpoint screenshots the port writes
(DW3_REPLAY_OUT) and compares them to reference captures kept OUTSIDE git.
Scenarios (tools/replay/manifest.py) are one or more phases; the phases of a
scenario share one memory-card dir, every scenario starts from a fresh one.

Per phase the runner issues a process verdict that is separate from the image
diff: CRASH (killed by a signal), EXIT (non-zero exit status), HANG (no game
frame progress for --stall seconds), TIMEOUT, NOEND (exited 0 before the
recording's end frame). Soak scenarios also get a memory verdict from the
process RSS. Exit status 0 = every check passed.

Only PIL is needed (python3-pil). Xvfb and ImageMagick `import` must be on PATH.
"""
from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import json
import os
import re
import shutil
import subprocess
import sys
import signal
import statistics
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageChops

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest  # noqa: E402
import areas  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCEN_DIR = Path(__file__).resolve().parent / "scenarios"
# P6.24 DW3_VERSION=both: one binary for both regions (--bin-both). The runner
# picks the region of each phase with DW3_REGION; nothing else changes, so the
# captures are compared with the references of the plain builds.
BOTH_BIN = None

# Per-channel tolerance (0-255) below which two pixels count as equal, and the
# fraction of differing pixels a checkpoint may have. Same-machine runs are
# byte-identical (see docs/P6.15_REPLAY_TESTS.md); the slack exists for another
# GPU driver / Mesa version, which shifts a few edge pixels by a few levels.
# 8/255 ignores that rounding noise; 0.1% of the 1280x960 window (~1230 px)
# is far smaller than any real UI change (one menu label is ~5000 px) yet
# above resampling noise.
CHANNEL_TOL = 8
MAX_DIFF_RATIO = 0.001
MAX_REPLAY_FRAME = (1 << (8 * ctypes.sizeof(ctypes.c_uint))) - 1

SCENARIOS = manifest.SCENARIOS
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


def mem_available_mib() -> int | None:
    """MemAvailable from /proc/meminfo in MiB (None where unreadable)."""
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    return int(line.split()[1]) // 1024
    except OSError:
        return None
    return None


# One scenario = a dw3-pc (RSS ~200 MiB, soak more), its Xvfb and the PNG diffing.
MIB_PER_JOB = 700


def auto_jobs(cpus: int | None = None, avail_mib: int | None = None) -> int:
    """--jobs auto: half the cores (the game thread and the mixer need a core
    each), at most 8, and no more than the free RAM can hold."""
    cpus = cpus if cpus is not None else (os.cpu_count() or 2)
    avail = avail_mib if avail_mib is not None else mem_available_mib()
    n = max(1, min(8, cpus // 2))
    if avail:
        n = min(n, max(1, avail // MIB_PER_JOB))
    return n


def wait_for_memory(min_avail_mib: int, announce, limit_s: int = 900) -> None:
    """RAM guard: hold a scenario back while MemAvailable is below the floor
    (the owner's desktop apps come and go). Gives up waiting after limit_s."""
    if min_avail_mib <= 0:
        return
    t0, told = time.time(), False
    while True:
        avail = mem_available_mib()
        if avail is None or avail >= min_avail_mib or time.time() - t0 > limit_s:
            return
        if not told:
            announce(f"waiting for memory: {avail} MiB available < {min_avail_mib} MiB")
            told = True
        time.sleep(5)


def changed_files(base: str | None) -> list[str]:
    """Files touched by the branch (BASE...HEAD) plus the working tree."""
    def git(*args):
        r = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True)
        return r.stdout.splitlines() if r.returncode == 0 else []
    if not base:
        base = next((b for b in ("origin/main", "main")
                     if subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--verify", "-q", b],
                                       capture_output=True).returncode == 0), "HEAD")
    files = set(git("diff", "--name-only", "--no-renames", f"{base}...HEAD"))
    files |= set(git("diff", "--name-only", "--no-renames", "HEAD"))
    files |= set(git("ls-files", "--others", "--exclude-standard"))
    return sorted(files)


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
        c = c.expanduser().resolve()
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


# --- process verdicts -------------------------------------------------------

FRAME_RE = re.compile(rb"dw3-replay: (?:frame|end at frame|shot \S+ frame=)\s*=?(\d+)")
RESTORE_RE = re.compile(r"dw3: overlay restore file=0x([0-9a-f]+) kind=(mode|sub|unknown)")
# Memory sanity for soak runs (see docs/P6.15_REPLAY_TESTS.md): RSS may not
# grow monotonically by more than this over the analysed window.
RSS_GROWTH_MIB = 48
RSS_SAMPLE_SECS = 5.0
DEFAULT_STALL = 120  # seconds without game-frame progress => HANG


def rss_kib(pid: int) -> int | None:
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except OSError:
        return None
    return None


def rss_verdict(samples, from_frame: int):
    """samples: [(frame, rss_kib)]. Returns (ok, detail).

    Window = samples at game frame >= from_frame, split into four equal
    chunks; the chunk medians are compared. FAIL when they rise strictly
    chunk over chunk (monotonic growth) AND the last median is more than
    RSS_GROWTH_MIB above the first. Needs 8+ samples, else the check is
    skipped (reported, not failed).
    """
    win = [(f, r) for f, r in samples if f >= from_frame and r]
    if not samples:
        return True, "no RSS samples"
    peak = max(r for _, r in samples) / 1024
    if len(win) < 8:
        return True, f"peak {peak:.0f} MiB (window too short for a growth check)"
    q = len(win) // 4
    med = [statistics.median(r for _, r in win[i * q:(i + 1) * q if i < 3 else len(win)])
           for i in range(4)]
    growth = (med[3] - med[0]) / 1024
    mono = med[0] < med[1] < med[2] < med[3]
    detail = (f"RSS peak {peak:.0f} MiB, window medians "
              + "/".join(f"{m / 1024:.0f}" for m in med)
              + f" MiB, growth {growth:+.1f} MiB (limit {RSS_GROWTH_MIB}, monotonic={mono})")
    return not (mono and growth > RSS_GROWTH_MIB), detail


def parse_restores(text: str):
    return [(int(m.group(1), 16), m.group(2)) for m in RESTORE_RE.finditer(text)]


def check_restores(log_text: str, expect):
    """Returns a list of problems: unknown overlay restores, missing/out of
    order expected restores (subsequence match)."""
    got = parse_restores(log_text)
    problems = []
    unknown = [f for f, k in got if k == "unknown"]
    if unknown:
        problems.append("unknown overlay restore(s): " +
                        ", ".join(f"0x{f:x}" for f in unknown[:5]))
    it = iter(got)
    for want in expect:
        if not any(g == want for g in it):
            problems.append(f"overlay restore 0x{want[0]:x} kind={want[1]} missing or out of order")
            break
    return problems


def check_log_expect(expect, phase_idx: int, text: str, forbid=()):
    """Problems for the manifest's expect_log entries of one phase: each
    (phase index, regex) must match somewhere in that phase's log, and no
    forbid_log regex may."""
    return ([f"log line /{rx}/ not found" for idx, rx in expect
             if idx == phase_idx and not re.search(rx, text)] +
            [f"log line /{rx}/ must not appear" for idx, rx in forbid
             if idx == phase_idx and re.search(rx, text)])


def write_seed_cfg(cfgdir: Path, cfg):
    """Seed port_options.cfg (P9.4 format) with the manifest's keys."""
    lines = ["# seeded by the replay runner", "version=1"]
    lines += [f"{k}={v}" for k, v in cfg]
    lines.append("end=1")
    (cfgdir / "port_options.cfg").write_text("\n".join(lines) + "\n")


def run_phase(rec: Path, version, binary, runtime, display, outdir: Path, memdir: Path,
              stamp, label, timeout, stall, turbo, sample_rss=True, cfgdir: Path | None = None,
              extra_env=(), allow_early_exit=False, launch=None):
    """One process run. Returns a dict with rc/verdict/seconds/frames/rss/log."""
    outdir.mkdir(parents=True, exist_ok=True)
    xdg = Path(tempfile.mkdtemp(prefix="dw3-replay-xdg-"))
    os.chmod(xdg, 0o700)
    env = dict(os.environ, DISPLAY=display, XDG_RUNTIME_DIR=str(xdg),
               SDL_AUDIODRIVER="dummy", DW3_RUNTIME=str(runtime),
               DW3_MEMCARD_DIR=str(memdir),
               DW3_REPLAY=str(rec),
               DW3_REPLAY_OUT=str(outdir), DW3_REPLAY_XSHOT="1")
    for k in ("DW3_AUTO_PAD", "DW3_REPLAY_SOURCE", "DW3_BUTTON_MAP", "DW3_REPLAY_RECORD", "DW3_PC_INTERNAL_RES", "DW3_TURBO",
              "DW3_TURBO_MUSIC", "DW3_TURBO_MUSIC_CLOCK", "DW3_CONFIG_DIR", "DW3_WIDESCREEN", "DW3_MUSIC_VOLUME", "DW3_SFX_VOLUME",
              "DW3_ENCOUNTER_RATE", "DW3_TEXT_SPEED", "DW3_START_MODE", "DW3_PROGRESS",
              "DW3_FORCE_NPC_ACTION", "DW3_OVERLAY_STRICT", "DW3_FORCE_ENCOUNTER",
              "DW3_FORCE_PARTY", "DW3_FORCE_WIN", "DW3_AUTO_DOOR_MODE",
              "DW3_AUTO_DOOR_FRAMES", "DW3_LANGUAGE", "DW3_FORCE_FORM", "DW3_FORCE_NICKNAME",
              "DW3_FORCE_TECH", "DW3_FORCE_HIT", "DW3_FORCE_STEAL", "DW3_FORCE_ENEMY_ITEM",
              "DW3_FORCE_ENEMY_HP", "DW3_EXTRA_TECHS", "DW3_FORCE_ITEMS", "DW3_FORCE_BATTLE_BAG", "DW3_MAP_VERIFY", "DW3_MAP_PRECOMPUTE", "DW3_MAP_SYNC", "DW3_FORCE_CARDS", "DW3_PACING_TRACKED",
              "DW3_FILE_CACHE_SLOTS", "DW3_MODS_DIR", "DW3_MOD_PACK", "DW3_FORCE_MENU", "DW3_CHEATS_ARM", "DW3_STAGE_SWEEP", "DW3_STAGE_SWEEP_EXIT", "DW3_STAGE_SWEEP_FRAMES", "DW3_STAGE_SWEEP_ONLY", "DW3_STAGE_SWEEP_DIR", "DW3_STAGE_SWEEP_HANG_MS", "DW3_FORCE_EXIT", "DW3_START_MODE_EVERY", "DW3_WALKTHROUGH_PROBE",
              "DW3_MEMCARD_IMPORT_DIR", "DW3_CROSS_IMPORT", "DW3_REGION", "DW3_REGION_PICK",
              "DW3_REGION_PICK_KEYS", "DW3_SAVE_PROFILE", "DW3_RUNTIME_BASE", "DW3_PC_BIN_US", "DW3_PC_BIN_EU"):
        env.pop(k, None)
    if BOTH_BIN is not None and launch is None and Path(binary).resolve() == BOTH_BIN:
        env["DW3_REGION"] = version  # P6.24: the one binary, told which game copy to run
    if launch is not None:  # P6.24: the launcher picks the runtime and the binary
        env.pop("DW3_RUNTIME", None)
        env.update(DW3_RUNTIME_BASE=str(launch["base"]),
                   DW3_PC_BIN_US=str(launch["us"]), DW3_PC_BIN_EU=str(launch["eu"]))
    if cfgdir is not None:  # P9.4: never read or write the owner's port_options.cfg
        env["DW3_CONFIG_DIR"] = str(cfgdir)
    if turbo:
        env["DW3_TURBO"] = str(turbo)
        # P6.36: the recordings that run in turbo were made with the music
        # ticking once per game frame (Turbo music = Fast); the scenarios of the
        # new gate set DW3_TURBO_MUSIC=0 in their own env.
        env["DW3_TURBO_MUSIC"] = "1"
    env.update(dict(extra_env))  # manifest Scenario.env (debug aids)
    log = outdir / f"{stamp}-{label}.log"  # rule 9: timestamped logs kept
    t0 = time.time()
    last_frame, last_progress = 0, t0
    rss, verdict, detail = [], "OK", ""
    with open(log, "wb") as lf:
        proc = subprocess.Popen([str(binary)], env=env, stdout=lf, stderr=subprocess.STDOUT,
                                cwd=outdir, start_new_session=True)
        pos, next_rss = 0, t0
        while True:
            try:
                proc.wait(1.0)
            except subprocess.TimeoutExpired:
                pass
            now = time.time()
            with open(log, "rb") as rf:  # new log bytes -> game-frame progress
                rf.seek(pos)
                chunk = rf.read()
                pos += len(chunk)
            for m in FRAME_RE.finditer(chunk):
                f = int(m.group(1))
                if f > last_frame:
                    last_frame, last_progress = f, now
            if sample_rss and now >= next_rss and proc.poll() is None:
                r = rss_kib(proc.pid)
                if r:
                    rss.append((last_frame, r))
                next_rss = now + RSS_SAMPLE_SECS
            if proc.poll() is not None:
                break
            if now - t0 > timeout:
                verdict, detail = "TIMEOUT", f"over {timeout}s (last frame {last_frame})"
            elif now - last_progress > stall:
                verdict, detail = "HANG", f"no game-frame progress for {stall}s (last frame {last_frame})"
            if verdict != "OK":
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
                break
    rc = proc.returncode
    text = log.read_bytes()
    if verdict == "OK":
        for m in FRAME_RE.finditer(text[pos:]):
            last_frame = max(last_frame, int(m.group(1)))
        if rc is not None and rc < 0:
            try:
                sig = signal.Signals(-rc).name
            except ValueError:
                sig = str(-rc)
            verdict, detail = "CRASH", f"killed by {sig} (last frame {last_frame})"
        elif rc != 0:
            verdict, detail = "EXIT", f"exit status {rc} (last frame {last_frame})"
        elif b"dw3-replay: end at frame" not in text and not allow_early_exit:
            verdict, detail = "NOEND", f"exited 0 before the recording's end (last frame {last_frame})"
    shutil.rmtree(xdg, ignore_errors=True)
    return dict(rc=rc, verdict=verdict, detail=detail, secs=time.time() - t0,
                frame=last_frame, rss=rss, log=log)


def shot_names(name):
    """Checkpoint names of a scenario (all phases, in order), validated."""
    sc = manifest.by_name().get(name)
    phases = sc.phases if sc else (name,)
    shots, seen_names = [], set()
    for phase in phases:
        rec = SCEN_DIR / f"{phase}.rec"
        if not rec.is_file():
            raise ValueError(f"missing recording: {rec}")
        previous_frame = -1
        for number, line in enumerate(rec.read_text().splitlines(), start=1):
            fields = line.split("#", 1)[0].split()
            if not fields or fields[0] != "shot":
                continue
            if len(fields) != 3 or not fields[1].isdigit():
                raise ValueError(f"{rec}:{number}: malformed shot directive")
            frame = int(fields[1])
            if frame < 1 or frame > MAX_REPLAY_FRAME:
                raise ValueError(
                    f"{rec}:{number}: checkpoint frame must be in 1..{MAX_REPLAY_FRAME}")
            if frame <= previous_frame:
                raise ValueError(f"{rec}:{number}: checkpoint frames must increase strictly")
            previous_frame = frame
            if not re.fullmatch(r"[A-Za-z0-9_-]+", fields[2]):
                raise ValueError(f"{rec}:{number}: unsafe checkpoint name {fields[2]!r}")
            if fields[2] in seen_names:
                raise ValueError(f"{rec}:{number}: duplicate checkpoint name {fields[2]!r}")
            seen_names.add(fields[2])
            shots.append(fields[2])
    if not shots and not (sc and sc.log_only):
        raise ValueError(f"scenario has no checkpoints: {name}")
    if shots and sc and sc.log_only:
        raise ValueError(f"log-only scenario has checkpoints: {name}")
    return shots


def mutate_save(path: Path, version: str, spec: str):
    """P6.25: edit slot 0 of a card the game wrote, for the refusal scenarios.
    "progress:<n>" sets GameState.progress and "badsum" flips the checksum
    byte; the slot checksum (XOR of bytes 4 .. GAME_SAVE_SIZE) is redone for
    progress. Raises if the save is not there."""
    size = {"us": 0x26BC, "eu": 0x26C4}[version]
    data = bytearray(path.read_bytes())
    base = 0x300
    kind, _, arg = spec.partition(":")
    if kind == "progress":
        data[base + 0x263C:base + 0x2640] = int(arg, 0).to_bytes(4, "little")
        x = 0
        for b in data[base + 4:base + size]:
            x ^= b
        data[base] = x
    elif kind == "badsum":
        data[base] ^= 0xA5
    else:
        raise ValueError(f"unknown save mutation {spec!r}")
    path.write_bytes(bytes(data))


def make_launch_base(sc, by_version, launcher) -> tuple[Path, dict]:
    """P6.24: a runtime base folder holding symlinks to the runtimes in
    `sc.launch` ("us", "eu", "us+eu"), and the launcher's view of the binaries."""
    base = Path(tempfile.mkdtemp(prefix="dw3-replay-base-"))
    for v in sc.launch.split("+"):
        (base / v).symlink_to(by_version[v][1])
    return base, dict(base=base, us=by_version["us"][0], eu=by_version["eu"][0])


def run_scenario(sc, binary, runtime, out: Path, stamp, label, stall, timeout_scale=1.0,
                 no_turbo=False, by_version=None, launcher=None):
    """Run all phases of one scenario under its own Xvfb. Returns
    (phase_results, sdir). Phases share one memcard dir. P6.25: a scenario with
    `phase_versions` runs each phase on its own disc (binary and runtime from
    `by_version`), with one card folder per region; the phase gets
    DW3_MEMCARD_IMPORT_DIR = the other region's folder."""
    sdir = out / label
    sdir.mkdir(parents=True, exist_ok=True)
    memdir = Path(tempfile.mkdtemp(prefix="dw3-replay-mc-"))
    cfgdir = Path(tempfile.mkdtemp(prefix="dw3-replay-cfg-"))   # shared by the phases
    if sc.cfg:
        write_seed_cfg(cfgdir, sc.cfg)
    if sc.mod_pack == "synthetic":  # MOD map upscale: gradients, no game art
        sys.path.insert(0, str(ROOT / "tools" / "mods"))
        import make_synthetic_pack
        make_synthetic_pack.make_pack(cfgdir / "mods", (sc.version,))
    elif sc.mod_pack == "synthetic-shared":  # P51 item 3: one shared map, both regions listed
        sys.path.insert(0, str(ROOT / "tools" / "mods"))
        import make_synthetic_pack
        make_synthetic_pack.make_pack(cfgdir / "mods", ("us", "eu"), shared=True)
    elif os.environ.get("DW3_REPLAY_MODS"):  # a local aid: look at a real pack
        (cfgdir / "mods").symlink_to(Path(os.environ["DW3_REPLAY_MODS"]).resolve())
    xvfb, display = start_xvfb(sdir)
    results = []
    launch, base = None, None
    if sc.launch:
        base, launch = make_launch_base(sc, by_version, launcher)
        binary = launcher
    try:
        for i, phase in enumerate(sc.phases):
            pv, pbin, prt, pmem = sc.version, binary, runtime, memdir
            penv = tuple(sc.env)
            if sc.phase_versions:
                pv = sc.phase_versions[i]
                pbin, prt = by_version[pv]
                pmem = memdir / pv
                other = memdir / ("eu" if pv == "us" else "us")
                pmem.mkdir(exist_ok=True)
                other.mkdir(exist_ok=True)
                penv += (("DW3_MEMCARD_IMPORT_DIR", str(other)),)
            if sc.phase_env:
                penv += tuple(sc.phase_env[i])
            r = run_phase(SCEN_DIR / f"{phase}.rec", pv, pbin, prt, display,
                          sdir, pmem, stamp, f"{sc.name}-p{i + 1}" if len(sc.phases) > 1 else sc.name,
                          int(sc.timeout * timeout_scale), stall, 0 if no_turbo else sc.turbo,
                          sample_rss=True, cfgdir=cfgdir, extra_env=penv,
                          allow_early_exit=sc.quits, launch=launch)
            r["phase"] = phase
            results.append(r)
            if sc.phase_mutate and sc.phase_mutate[i] and r["verdict"] == "OK":
                mutate_save(pmem / "save0.bin", pv, sc.phase_mutate[i])
            if r["verdict"] != "OK":
                break  # later phases depend on this one
    finally:
        xvfb.terminate()
        try:
            xvfb.wait(5)
        except subprocess.TimeoutExpired:
            xvfb.kill()
        shutil.rmtree(memdir, ignore_errors=True)
        shutil.rmtree(cfgdir, ignore_errors=True)
        if base is not None:
            shutil.rmtree(base, ignore_errors=True)
    return results, sdir


def reference_path(refs: Path, name: str, ref_from: str, shot: str, ext: str,
                   update: bool, both: bool) -> Path:
    """Reference file of one checkpoint. On the combined (--bin-both) binary a
    `<refs>/<name>/both/` capture overrides the plain one (P6.51: the Region row
    of the Gameplay group exists only there); `--update` on that binary writes
    there, so the plain references are never touched by a both run."""
    if both:
        own = refs / name / "both" / f"{shot}.{ext}"
        if update or own.is_file():
            return own
    return refs / (name if update else (ref_from or name)) / f"{shot}.{ext}"


def judge_scenario(sc, results, sdir, refs: Path, checkpoints, update, labelname=None,
                   both: bool = False):
    """Rows (scenario, checkpoint, result, detail) for one finished scenario."""
    name = sc.name
    rows = []
    bad_proc = False
    expect_log = sc.expect_log_both if (both and sc.expect_log_both) else sc.expect_log
    for r in results:
        ph = f" {r['phase']}" if len(results) > 1 else ""
        if r["verdict"] != "OK":
            bad_proc = True
            rows.append((name, f"(process{ph})", r["verdict"], r["detail"]))
        else:
            rows.append((name, f"(process{ph})", "PASS",
                         f"{r['secs']:.0f}s, {r['frame']} game frames"))
    log_text = "\n".join(r["log"].read_text(errors="replace") for r in results)
    probs = check_restores(log_text, sc.expect_restore)
    for p in probs:
        rows.append((name, "(markers)", "FAIL", p))
    log_probs = 0
    for i, r in enumerate(results):
        for p in check_log_expect(expect_log, i, r["log"].read_text(errors="replace"),
                                  sc.forbid_log):
            log_probs += 1
            rows.append((name, f"(log p{i + 1})", "FAIL", p))
    if (expect_log or sc.forbid_log) and not log_probs and len(results) == len(sc.phases):
        rows.append((name, "(log)", "PASS",
                     f"{len(expect_log)} expected log line(s) found, "
                     f"{len(sc.forbid_log)} forbidden line(s) absent"))
    if not probs and (sc.expect_restore or results):
        rows.append((name, "(markers)", "PASS",
                     f"{len(parse_restores(log_text))} overlay restores, none unknown"))
    if sc.tier == "soak" or sc.rss_from_frame:
        samples = [s for r in results for s in r["rss"]]
        ok, detail = rss_verdict(samples, sc.rss_from_frame)
        rows.append((name, "(memory)", "PASS" if ok else "FAIL", detail))
    else:
        peaks = [max((x for _, x in r["rss"]), default=0) for r in results]
        if peaks and max(peaks):
            rows.append((name, "(memory)", "INFO", f"RSS peak {max(peaks) / 1024:.0f} MiB"))
    for shot in checkpoints:
        for ext in ARTIFACTS:
            got = sdir / f"{shot}.{ext}"
            ref = reference_path(refs, name, sc.ref_from, shot, ext, update, both)
            label = f"{shot}.{ext}"
            # P9.7: a checkpoint compared with another scenario's capture (the
            # excursion run must equal the plain run, shifted), never its own.
            mapped = next((m for m in sc.ref_map if m[0] == shot), None)
            if mapped is not None:
                ref = refs / mapped[1] / f"{mapped[2]}.{ext}"
                if update:
                    rows.append((name, label, "REF",
                                 f"no own reference: compared with {mapped[1]}/{mapped[2]}"))
                    continue
            if not got.is_file():
                rows.append((name, label, "FAIL",
                             "no capture (process did not get that far)" if bad_proc else "no capture"))
                continue
            if update:
                if bad_proc:
                    rows.append((name, label, "FAIL", "not writing a reference from a failed run"))
                    continue
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
                rows.append((name, label, "FAIL", f"{n} px differ ({100 * n / total:.3f}%) -> {diff}"))
            else:
                rows.append((name, label, "PASS", f"{n} px differ"))
    return rows


def determinism_rows(sc, dir_a: Path, dir_b: Path, checkpoints):
    """Two runs of the same scenario must produce identical captures."""
    rows = []
    for shot in checkpoints:
        for ext in ARTIFACTS:
            a, b = dir_a / f"{shot}.{ext}", dir_b / f"{shot}.{ext}"
            label = f"{shot}.{ext}"
            if not a.is_file() or not b.is_file():
                rows.append((sc.name, label, "FAIL", "missing capture in one of the runs"))
                continue
            if ext == "ppm":
                same = a.read_bytes() == b.read_bytes()
                rows.append((sc.name, label, "PASS" if same else "FAIL",
                             "byte-identical" if same else "PPM differs between runs"))
            else:
                n, total, err = compare_images(a, b, None)
                ok = err is None and n == 0
                rows.append((sc.name, label, "PASS" if ok else "FAIL",
                             "0 px differ" if ok else (err or f"{n} px differ between runs")))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--update", action="store_true", help="(re)generate references from this run")
    ap.add_argument("--only", action="append", help="scenario name (repeatable)")
    ap.add_argument("--quick", action="store_true",
                    help="title-menu scenarios only (the original fast set, ~1 minute)")
    ap.add_argument("--soak", action="store_true",
                    help="also run the soak scenarios (15-20 min of game time each)")
    ap.add_argument("--list", action="store_true", help="list scenarios and exit")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the selected scenarios (--changed / --quick / --only) and exit")
    ap.add_argument("--twice", action="store_true",
                    help="run every selected scenario twice and require identical captures "
                         "(determinism check; needs no references)")
    ap.add_argument("--no-turbo", action="store_true", help="ignore the per-scenario DW3_TURBO speed")
    ap.add_argument("--jobs", default="3",
                    help="scenarios run in parallel: a number (default 3), or 'auto' = half "
                         "the cores, at most 8, limited by free RAM. Agents on the owner's "
                         "desktop use 2 (AGENTS.md); CI and a dedicated box can use auto")
    ap.add_argument("--min-avail-mib", type=int,
                    default=int(os.environ.get("DW3_REPLAY_MIN_AVAIL_MIB", "1536")),
                    help="RAM guard: do not start a scenario while MemAvailable is below this "
                         "(MiB, default 1536; 0 = off)")
    ap.add_argument("--fail-fast", action="store_true",
                    help="after the first failing scenario, do not start the rest "
                         "(they are reported as SKIP)")
    ap.add_argument("--changed", nargs="?", const="", metavar="BASE",
                    help="pre-PR aid: run only the scenarios the files changed since BASE "
                         "(default origin/main, plus the working tree) can affect, plus the "
                         "quick tier and a smoke set. Anything unmapped selects the full "
                         "tier. CI and release checks run the full tier, not this")
    ap.add_argument("--changed-files", nargs="+", metavar="PATH",
                    help="like --changed with an explicit file list (no git)")
    ap.add_argument("--stall", type=int, default=DEFAULT_STALL,
                    help=f"seconds without game-frame progress before HANG (default {DEFAULT_STALL})")
    ap.add_argument("--timeout-scale", type=float, default=1.0)
    ap.add_argument("--refs", default=os.environ.get(
        "DW3_REPLAY_REFS", str(Path.home() / ".cache/dw3-switch/replay-refs")),
        help="reference dir, OUTSIDE git (default ~/.cache/dw3-switch/replay-refs)")
    ap.add_argument("--out", default=os.environ.get("DW3_REPLAY_RESULTS", str(ROOT / "tmp/replay")),
                    help="results dir (captures, diffs, logs)")
    ap.add_argument("--bin-us", default=os.environ.get("DW3_REPLAY_BIN_US"))
    ap.add_argument("--bin-eu", default=os.environ.get("DW3_REPLAY_BIN_EU"))
    ap.add_argument("--bin-both", default=os.environ.get("DW3_REPLAY_BIN_BOTH"),
                    help="P6.24: ONE dw3-pc built with DW3_VERSION=both, used for every "
                         "scenario of both regions (DW3_REGION is set per phase)")
    ap.add_argument("--launcher", default=os.environ.get("DW3_REPLAY_LAUNCHER"),
                    help="P6.24 region launcher binary (the 'single' scenarios)")
    ap.add_argument("--single", action="store_true",
                    help="run the P6.24 'single' scenarios (through the launcher) only")
    ap.add_argument("--runtime-us")
    ap.add_argument("--runtime-eu")
    ap.add_argument("--build", action="store_true", help="configure+build dw3-pc (us, eu) under --out")
    ap.add_argument("--scen-dir", help="recordings dir (default tools/replay/scenarios)")
    ap.add_argument("--timeout", type=int, default=None,
                    help="override every scenario's per-phase timeout (seconds)")
    a = ap.parse_args()

    global SCEN_DIR
    if a.scen_dir:
        SCEN_DIR = Path(a.scen_dir).expanduser().resolve()
    if a.list:
        for sc in SCENARIOS:
            print(f"{sc.name:<22} {sc.version} {sc.tier:<5} phases={len(sc.phases)} {sc.note}")
        return 0
    names = {sc.name for sc in SCENARIOS}
    if a.only:
        unknown = sorted(set(a.only) - names)
        if unknown:
            ap.error(f"unknown scenario(s): {', '.join(unknown)}")
        selected = set(a.only)
    elif a.changed is not None or a.changed_files:
        files = a.changed_files if a.changed_files else changed_files(a.changed or None)
        sel = areas.select(files, SCENARIOS)
        selected = sel.names
        print(f"--changed: {len(files)} changed file(s) -> {len(selected)} scenario(s)"
              f"{' (full tier: ' + 'an unmapped or engine-wide file' + ')' if sel.everything else ''}")
        for path, why in sel.reasons[:40]:
            print(f"  {path}: {why}")
        if len(sel.reasons) > 40:
            print(f"  ... {len(sel.reasons) - 40} more")
        for g in sorted(set(sel.unmapped_globs)):
            print(f"  note: rule pattern {g!r} matches no scenario (areas.py is stale?)")
    elif a.single:
        selected = {sc.name for sc in SCENARIOS if sc.tier == "single"}
    elif a.quick:
        selected = {sc.name for sc in SCENARIOS if sc.tier == "quick"}
    else:
        tiers = {"quick", "full"} | ({"soak"} if a.soak else set())
        selected = {sc.name for sc in SCENARIOS if sc.tier in tiers}
    if not selected:
        ap.error("no scenarios selected")
    if a.dry_run:
        print(f"{len(selected)} of {len(SCENARIOS)} scenarios selected:")
        print("\n".join(f"  {sc.name}" for sc in SCENARIOS if sc.name in selected))
        return 0
    try:
        checkpoints = {n: shot_names(n) for n in selected}
    except (OSError, ValueError) as exc:
        ap.error(str(exc))
    if a.timeout:
        for sc in SCENARIOS:
            object.__setattr__(sc, "timeout", a.timeout)

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = Path(a.out).expanduser().resolve() / stamp
    refs = Path(a.refs).expanduser().resolve()
    if a.update:
        try:
            refs.relative_to(ROOT)
        except ValueError:
            pass
        else:
            ap.error(f"--update references must be outside the repository: {refs}")
    out.mkdir(parents=True, exist_ok=True)
    refs.mkdir(parents=True, exist_ok=True)

    global BOTH_BIN
    if a.bin_both:
        BOTH_BIN = Path(a.bin_both).expanduser().resolve()
        a.bin_us = a.bin_eu = str(BOTH_BIN)
    bins, runtimes = {}, {}
    for v in ("us", "eu"):
        arg = getattr(a, f"bin_{v}")
        if a.build and not arg:
            arg = str(build_host(v, out))
        if not arg:
            for c in (ROOT / f"build/pc{'' if v == 'us' else '-eu'}/dw3-pc",):
                if c.is_file():
                    arg = str(c)
        bins[v] = Path(arg).expanduser().resolve() if arg else None
        runtimes[v] = find_runtime(v, getattr(a, f"runtime_{v}"))

    launcher = Path(a.launcher).expanduser().resolve() if a.launcher else None
    todo = [sc for sc in SCENARIOS if sc.name in selected]
    # Long scenarios first so the parallel pool finishes earlier.
    order = sorted(todo, key=lambda sc: -sc.timeout * len(sc.phases))
    lock = threading.Lock()
    rows_by = {}
    stop = threading.Event()    # --fail-fast: set by the first failing scenario
    jobs = auto_jobs() if str(a.jobs).lower() == "auto" else max(1, int(a.jobs))
    print(f"replay: {len(todo)} scenario(s), {jobs} in parallel"
          f"{' (auto)' if str(a.jobs).lower() == 'auto' else ''}", flush=True)

    def say(msg):
        with lock:
            print(msg, flush=True)

    def job(sc):
        if a.fail_fast and stop.is_set():
            return sc.name, [(sc.name, "-", "SKIP", "not started: --fail-fast after an earlier failure")]
        wait_for_memory(a.min_avail_mib, lambda m: say(f"[{sc.name}] {m}"))
        if sc.tier == "both" and BOTH_BIN is None:
            return sc.name, [(sc.name, "-", "SKIP", "needs the combined binary (--bin-both)")]
        if sc.launch and not (launcher and launcher.is_file()):
            return sc.name, [(sc.name, "-", "SKIP", "missing launcher binary (--launcher)")]
        for v in sorted(set(sc.phase_versions) | {sc.version} | set(sc.launch.split("+") if sc.launch else ())):
            if not bins[v] or not bins[v].is_file() or runtimes[v] is None:
                return sc.name, [(sc.name, "-", "SKIP", f"missing {'binary' if not bins[v] else 'runtime'} for {v}")]
        rows = []
        runs = []
        for run in (("a", "b") if a.twice else ("",)):
            label = f"{sc.name}-{run}" if run else sc.name
            results, sdir = run_scenario(sc, bins[sc.version], runtimes[sc.version], out, stamp,
                                         label, a.stall, a.timeout_scale, a.no_turbo,
                                         {v: (bins[v], runtimes[v]) for v in ("us", "eu")}, launcher)
            runs.append((results, sdir))
            ok = all(r["verdict"] == "OK" for r in results)
            with lock:
                print(f"[{label}] done in {sum(r['secs'] for r in results):.0f}s "
                      f"{'ok' if ok else results[-1]['verdict'] + ' ' + results[-1]['detail']}", flush=True)
        # Run `a` is the run judged against the references (or written by --update).
        rows += judge_scenario(sc, runs[0][0], runs[0][1], refs, checkpoints[sc.name], a.update,
                               both=bool(a.bin_both))
        if a.twice:
            if all(r["verdict"] == "OK" for res, _ in runs for r in res):
                rows += determinism_rows(sc, runs[0][1], runs[1][1], checkpoints[sc.name])
            else:
                rows.append((sc.name, "(determinism)", "FAIL", "the second run failed; no comparison"))
        if a.fail_fast and any(r[2] not in ("PASS", "REF", "INFO") for r in rows):
            stop.set()
        return sc.name, rows

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for name, rows in pool.map(job, order):
            rows_by[name] = rows
    rows = [r for sc in todo for r in rows_by.get(sc.name, [])]

    w = max(len(r[0]) for r in rows) if rows else 8
    lines = [f"{'scenario':<{w}}  {'checkpoint':<20} result   detail"]
    for r in rows:
        lines.append(f"{r[0]:<{w}}  {r[1]:<20} {r[2]:<8} {r[3]}")
    bad = [r for r in rows if r[2] not in ("PASS", "REF", "INFO")]
    lines.append(f"\n{len(rows)} checks, {len(bad)} failed/skipped; results in {out}")
    text = "\n".join(lines)
    print(text)
    (out / "report.txt").write_text(text + "\n")
    (out / "report.json").write_text(json.dumps(rows, indent=1))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
