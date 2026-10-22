#!/usr/bin/env python3
"""P6.17 evidence: run the PC build with a Music / SFX volume pair and print
the per-bus mix before and after the gain (docs/P6.17_VOLUME.md).

  tools/audio_bus_probe/probe.py --version us --out DIR [--pair 10,10 --pair 5,10 ...]

Each run replays a short pad recording (title music, then cursor moves on the
title menu for sound effects) under xvfb-run, with DW3_MUSIC_VOLUME /
DW3_SFX_VOLUME pinned and DW3_AUDIO_STATS=<frames> set, with the SDL disk
audio driver so the PCM is kept as DIR/<run>/audio.raw. The table lists, per
bus, the RMS before -> after the gain from the game's own `dw3-audio:` log,
and the RMS of the PCM after its first second. Needs the owner's runtime
(~/.cache/dw3-switch/runtime/<ver>) and a built build/pc-<ver>/dw3-pc. The
recordings hold pad masks only; the raw PCM is game audio: do not commit it.
"""
import argparse
import array
import math
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
START, DOWN, UP = 0x0008, 0x0040, 0x0010


def recording(version):
    lines = [f"# P6.17 audio probe ({version})"]
    if version == "us":
        presses, first, end = [20, 210], 330, 600
    else:  # EU: confirm English, skip splash/FMV, PRESS START
        presses, first, end = [110, 160, 190, 220, 280], 400, 680
    for f in presses:
        lines += [f"pad {f} 0x{START:04x}", f"pad {f + 5} 0x0000"]
    for i in range(6):  # cursor moves on the title menu: effects over the music
        f = first + 30 * i
        lines += [f"pad {f} 0x{(DOWN if i % 2 == 0 else UP):04x}", f"pad {f + 5} 0x0000"]
    lines.append(f"end {end}")
    return "\n".join(lines) + "\n", end - 10


def run(version, music, sfx, out, binary, runtime):
    rec, window = recording(version)
    out.mkdir(parents=True, exist_ok=True)
    (out / "probe.rec").write_text(rec)
    with tempfile.TemporaryDirectory(prefix="dw3-probe-") as tmp:
        tmp = Path(tmp)
        for d in ("cfg", "mem", "xdg"):
            (tmp / d).mkdir(mode=0o700)
        env = dict(os.environ, DW3_RUNTIME=str(runtime), DW3_MEMCARD_DIR=str(tmp / "mem"),
                   DW3_CONFIG_DIR=str(tmp / "cfg"), DW3_REPLAY=str(out / "probe.rec"),
                   DW3_REPLAY_OUT=str(out), XDG_RUNTIME_DIR=str(tmp / "xdg"),
                   SDL_AUDIODRIVER="disk", SDL_AUDIO_DISK_OUTPUT_FILE=str(out / "audio.raw"),
                   DW3_AUDIO_STATS=str(window), DW3_MUSIC_VOLUME=str(music),
                   DW3_SFX_VOLUME=str(sfx))
        with open(out / "run.log", "wb") as log:
            subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1280x960x24 -nolisten tcp",
                            str(binary)], env=env, stdout=log, stderr=subprocess.STDOUT,
                           check=True, cwd=out)
    return (out / "run.log").read_text(errors="replace")


def summarize(log, raw):
    buses = {n: (int(i), int(o)) for n, i, o in re.findall(
        r"bus (\w+)\s+in_rms=(\d+) out_rms=(\d+)", log)}

    def cell(name):
        i, o = buses.get(name, (0, 0))
        return f"{i} -> {o} ({o / i:.3f})" if i else "no voices"
    pcm = array.array("h")
    pcm.frombytes(raw.read_bytes()[: raw.stat().st_size // 2 * 2])
    pcm = pcm[88200:]  # skip the first second
    rms = math.sqrt(sum(x * x for x in pcm) / max(len(pcm), 1))
    return cell("music"), cell("sfx"), rms


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", choices=("us", "eu"), default="us")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--pair", action="append", default=[],
                    help="MUSIC,SFX (repeatable); default 10,10 5,10 10,5 0,0")
    ap.add_argument("--bin", type=Path)
    ap.add_argument("--runtime", type=Path)
    a = ap.parse_args()
    binary = a.bin or ROOT / f"build/pc-{a.version}/dw3-pc"
    runtime = a.runtime or Path.home() / f".cache/dw3-switch/runtime/{a.version}"
    print("| run | Music bus in -> out (ratio) | SFX bus in -> out (ratio) | PCM RMS |")
    print("|---|---|---|---|")
    for pair in a.pair or ["10,10", "5,10", "10,5", "0,0"]:
        m, s = (int(x) for x in pair.split(","))
        out = a.out / f"{a.version}-m{m}-s{s}"
        log = run(a.version, m, s, out, binary, runtime)
        mc, sc, rms = summarize(log, out / "audio.raw")
        print(f"| {a.version.upper()} {m} / {s} | {mc} | {sc} | {rms:.0f} |")


if __name__ == "__main__":
    main()
