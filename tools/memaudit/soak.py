#!/usr/bin/env python3
"""P5.1 PC memory soak driver (docs/P5.1_MEMORY.md "PC soak").

One dw3-pc process plays a long scripted session under its own Xvfb, nice 10,
and logs the PC equivalent of the Switch `mem dbg:` line every 600 frames
(platforms/pc/src/mem_sampler.c). The session:

  phase 0  a short run saves a game on a fresh card (the forced save screen of
           the P6.25 `xr-save` scenario), so the soak has a slot to auto-save;
  phase 1  the soak: Continue that save, then a pad loop forever. The loop
           walks the field, taps Cross (dialogue, battle menus), opens and
           closes the field menu, and the host re-arms the Cheats one-shots
           every --arm-every frames in turn: Warp to another area (area change
           -> auto-save, stage reload), Battle now (random battle, forced
           Kunemon vs Kotemon, won with Cross), Money and Give item.

Needs: Xvfb, a dw3-pc build of the region, the region's runtime folder.
Everything it writes goes under --out (use /mnt/cabal/tmp/...); copy the CSV
and logs to the evidence folder afterwards. Nothing here touches the owner's
port_options.cfg or memory cards.

  soak.py --version us --bin .../pc-us/dw3-pc --runtime ~/.cache/dw3-switch/runtime/us \
          --out /mnt/cabal/tmp/dw3-p51/soak-us --hours 2
"""
import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCEN = ROOT / "tools/replay/scenarios"
START, DOWN, CROSS, TRIANGLE = 0x0008, 0x0040, 0x4000, 0x1000
UP, RIGHT, LEFT = 0x0010, 0x0020, 0x0080
HOLD = 5

# Continue is entered at the end of these prefixes (frame of the last Cross of
# the Continue sequence in <v>-as-load.rec); the field has control after it.
CONTINUE_END = {"us": 1230, "eu": 1300}
LOOP_START = {"us": 1500, "eu": 1600}
FRAME_HZ = {"us": 59.94, "eu": 50.0}

# Re-armed in turn: areas with random battles (Bulk Swamp 0x233, Tyranno
# Valley 0x22B, Central Park 0x21D, Noise Desert 0x248, Wire Forest 0x21E),
# towns and the lab, with battles, money and items in between.
ARM_LIST = ("warp=0x233;battle_now;battle_now;money_10k;warp=0x206;"
            "item=47:7;warp=0x22B;battle_now;battle_now;warp=0x200;"
            "money_100k;warp=0x21D;battle_now;battle_now;warp=0x209;"
            "warp=0x248;battle_now;battle_now;item=47:7;warp=0x21E;"
            "battle_now;fill_bag;warp=0x233;battle_now")

SAVE_ENV = (("DW3_START_MODE", "0x20B"), ("DW3_PROGRESS", "6"),
            ("DW3_FORCE_MENU", "0xC00:0x80000000"))
SOAK_ENV = (("DW3_FORCE_ENCOUNTER", "34"), ("DW3_FORCE_PARTY", "0"),
            ("DW3_FORCE_ENEMY_HP", "60"))

SCRUB = ("DW3_AUTO_PAD", "DW3_REPLAY_RECORD", "DW3_PC_INTERNAL_RES", "DW3_TURBO",
         "DW3_CONFIG_DIR", "DW3_WIDESCREEN", "DW3_START_MODE", "DW3_PROGRESS",
         "DW3_FORCE_NPC_ACTION", "DW3_OVERLAY_STRICT", "DW3_FORCE_ENCOUNTER",
         "DW3_FORCE_PARTY", "DW3_FORCE_WIN", "DW3_AUTO_DOOR_MODE", "DW3_LANGUAGE",
         "DW3_FORCE_MENU", "DW3_CHEATS_ARM", "DW3_FORCE_EXIT", "DW3_START_MODE_EVERY",
         "DW3_FILE_CACHE_SLOTS", "DW3_REGION", "DW3_REGION_PICK")


def prefix_lines(version, cutoff):
    out = []
    for ln in (SCEN / f"{version}-as-load.rec").read_text().splitlines():
        p = ln.split()
        if ln.startswith("pad") and int(p[1]) <= cutoff + HOLD:
            out.append(ln)
    return out


def loop_cycle(f):
    """One pad cycle starting at frame f. Returns (lines, next_frame)."""
    L = []

    def hold(mask, n):
        nonlocal f
        L.append(f"pad {f} 0x{mask:04x}")
        f += n
        L.append(f"pad {f} 0x0000")

    def tap(mask, wait):
        nonlocal f
        L.append(f"pad {f} 0x{mask:04x}")
        L.append(f"pad {f + HOLD} 0x0000")
        f += HOLD + wait

    hold(UP, 150)
    hold(RIGHT, 120)
    hold(DOWN, 150)
    hold(LEFT, 120)
    for _ in range(4):
        tap(CROSS, 60)
    tap(START, 80)          # field menu
    tap(CROSS, 120)         # first entry (Items)
    for _ in range(4):      # back out of whatever list is open
        tap(TRIANGLE, 70)
    hold(DOWN | LEFT, 90)
    hold(UP | RIGHT, 90)
    for _ in range(40):     # battle menus, reports, dialogue
        tap(CROSS, 20)
    return L, f


def make_rec(version, frames, path, shot_every=0):
    lines = [f"# {version.upper()}: P5.1 memory soak, {frames} frames (tools/memaudit/soak.py)"]
    lines += prefix_lines(version, CONTINUE_END[version])
    f = LOOP_START[version]
    while f < frames:
        cyc, f = loop_cycle(f)
        lines += cyc
    lines = [ln for ln in lines if ln.startswith("#") or int(ln.split()[1]) < frames]
    lines.append(f"pad {frames - 1} 0x0000")
    if shot_every:  # debugging aid: window grabs into DW3_REPLAY_OUT
        lines += [f"shot {n} s{n}" for n in range(LOOP_START[version], frames, shot_every)]
    lines.append(f"end {frames}")
    path.write_text("\n".join(lines) + "\n")


def start_xvfb(work):
    r, w = os.pipe()
    proc = subprocess.Popen(["Xvfb", "-displayfd", str(w), "-screen", "0", "1280x960x24",
                             "-nolisten", "tcp"], pass_fds=(w,), stdout=subprocess.DEVNULL,
                            stderr=open(work / "xvfb.log", "w"))
    os.close(w)
    buf = b""
    while not buf.endswith(b"\n"):
        c = os.read(r, 16)
        if not c:
            raise RuntimeError("Xvfb did not report a display")
        buf += c
    os.close(r)
    return proc, ":" + buf.decode().strip()


def write_cfg(cfgdir, pairs):
    cfgdir.mkdir(parents=True, exist_ok=True)
    lines = ["# seeded by tools/memaudit/soak.py", "version=1"]
    lines += [f"{k}={v}" for k, v in pairs]
    lines.append("end=1")
    (cfgdir / "port_options.cfg").write_text("\n".join(lines) + "\n")


def run(cmd_env, binary, log, nice, prefix=()):
    cmd = list(prefix) + ["nice", "-n", str(nice), str(binary)]
    with open(log, "wb") as lf:
        return subprocess.Popen(cmd, env=cmd_env, stdout=lf, stderr=subprocess.STDOUT)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True, choices=("us", "eu"))
    ap.add_argument("--bin", required=True)
    ap.add_argument("--runtime", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--hours", type=float, default=2.0,
                    help="game time of the soak loop, at the disc's frame rate")
    ap.add_argument("--turbo", type=int, default=0, help="DW3_TURBO speed (0 = 1x)")
    ap.add_argument("--uncapped", action="store_true",
                    help="no frame limiter and no turbo (Xvfb then runs ~2x real time)")
    ap.add_argument("--arm-every", type=int, default=2400)
    ap.add_argument("--log-every", type=int, default=600)
    ap.add_argument("--preload", help="libdw3_malloc_audit.so for the allocation audit")
    ap.add_argument("--audit-from", type=int, default=3000)
    ap.add_argument("--nice", type=int, default=10)
    ap.add_argument("--shot-every", type=int, default=0, help="debug: a screenshot every N frames")
    ap.add_argument("--no-autosave", action="store_true")
    ap.add_argument("--skip-save-phase", action="store_true")
    a = ap.parse_args()

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    memdir, cfgdir = out / "memcard", out / "config"
    shutil.rmtree(memdir, ignore_errors=True)
    shutil.rmtree(cfgdir, ignore_errors=True)
    memdir.mkdir()
    (out / "shots").mkdir(exist_ok=True)
    frames = int(a.hours * 3600 * FRAME_HZ[a.version])
    xdg = Path(tempfile.mkdtemp(prefix="dw3-soak-xdg-"))
    os.chmod(xdg, 0o700)
    xvfb, display = start_xvfb(out)
    base = {k: v for k, v in os.environ.items() if k not in SCRUB}
    base.update(DISPLAY=display, XDG_RUNTIME_DIR=str(xdg), SDL_AUDIODRIVER="dummy",
                DW3_RUNTIME=a.runtime, DW3_MEMCARD_DIR=str(memdir),
                DW3_CONFIG_DIR=str(cfgdir))
    try:
        if not a.skip_save_phase:
            rec0 = out / "phase0-save.rec"
            rec0.write_text((SCEN / f"{a.version}-xr-save.rec").read_text())
            write_cfg(cfgdir, ())
            env = dict(base, DW3_REPLAY=str(rec0), DW3_REPLAY_OUT=str(out / "shots"),
                       **dict(SAVE_ENV))
            p = run(env, a.bin, out / "phase0-save.log", a.nice)
            rc = p.wait(timeout=900)
            print(f"phase 0 (save) exit={rc}", flush=True)
            if rc != 0 or not any(memdir.iterdir()):
                print("phase 0 failed: no save", file=sys.stderr)
                return 1
        rec1 = out / "soak.rec"
        make_rec(a.version, frames, rec1, a.shot_every)
        write_cfg(cfgdir, (("cheat_enabled", 1),) + (() if a.no_autosave else (("autosave", 1),)))
        env = dict(base, DW3_REPLAY=str(rec1), DW3_REPLAY_OUT=str(out / "shots"),
                   DW3_MEM_LOG=str(out / "mem.csv"), DW3_MEM_LOG_EVERY=str(a.log_every),
                   DW3_SOAK_ARM_EVERY=str(a.arm_every), DW3_SOAK_ARM_LIST=ARM_LIST,
                   **dict(SOAK_ENV))
        if a.shot_every:
            env["DW3_REPLAY_XSHOT"] = "1"
        if not a.turbo and not a.uncapped:
            env["DW3_PC_FRAME_LIMIT"] = "1"  # true 1x; Xvfb has no blocking vsync
        if a.turbo:
            env["DW3_TURBO"] = str(a.turbo)
        if a.preload:
            env["LD_PRELOAD"] = a.preload
            env["DW3_MALLOC_AUDIT_OUT"] = str(out / "audit")
            env["DW3_MALLOC_AUDIT_FROM"] = str(a.audit_from)
            env["DW3_MALLOC_AUDIT_WINDOW"] = str(a.log_every)
        t0 = time.time()
        p = run(env, a.bin, out / "soak.log", a.nice)
        print(f"soak pid={p.pid} frames={frames} (~{a.hours} h game time, turbo={a.turbo or 1}) "
              f"display={display}", flush=True)

        def stop(*_):
            p.terminate()

        signal.signal(signal.SIGTERM, stop)
        rc = p.wait()
        print(f"soak exit={rc} wall={time.time() - t0:.0f}s", flush=True)
        return 0 if rc == 0 else 2
    finally:
        xvfb.terminate()
        try:
            xvfb.wait(5)
        except subprocess.TimeoutExpired:
            xvfb.kill()
        shutil.rmtree(xdg, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
