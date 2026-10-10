#!/usr/bin/env python3
"""Capture DuckStation vs dw3-pc frames for the same save/label (P3.9)."""

from __future__ import annotations

import argparse
import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

LABELS = ("title", "field", "battle")

DEFAULT_DUCKSTATION = Path("/mnt/windows/Jogos/emuladores/duckstation")
DEFAULT_DISC = Path(
    "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.cue"
)
DEFAULT_BIOS = Path.home() / ".local/share/duckstation/bios/scph1001.bin"
DS_DATA = Path.home() / ".local/share/duckstation"


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=(
            "Capture DuckStation (oracle) and optionally dw3-pc (subject) "
            "frames for P3.9. DuckStation path: -batch/-fastboot + F10 "
            "hotkey via xdotool (no screenshot CLI flag in DS 0.1-12074)."
        )
    )
    p.add_argument(
        "--label",
        required=True,
        choices=LABELS,
        help="scene label: title, field, or battle",
    )
    p.add_argument(
        "--runtime",
        type=Path,
        default=Path("runtime/us"),
        help="PC runtime data root (default: runtime/us)",
    )
    p.add_argument(
        "--save",
        type=Path,
        default=None,
        help="shared slot-0 .mcr path (field/battle; unused for boot title)",
    )
    p.add_argument(
        "--duckstation",
        type=Path,
        default=DEFAULT_DUCKSTATION if DEFAULT_DUCKSTATION.exists() else None,
        help="DuckStation executable (AppImage or binary)",
    )
    p.add_argument(
        "--disc",
        type=Path,
        default=DEFAULT_DISC if DEFAULT_DISC.exists() else None,
        help="PS1 .cue/.bin/.chd path for DuckStation boot",
    )
    p.add_argument(
        "--bios",
        type=Path,
        default=DEFAULT_BIOS,
        help="BIOS file or symlink DuckStation should see (default: data dir)",
    )
    p.add_argument(
        "--pc-bin",
        type=Path,
        default=Path("build/pc/dw3-pc"),
        help="dw3-pc binary (default: build/pc/dw3-pc)",
    )
    p.add_argument(
        "--out",
        type=Path,
        default=Path("build/compare"),
        help="output root for <label>/{duck,pc}.png (default: build/compare)",
    )
    p.add_argument(
        "--settle-seconds",
        type=float,
        default=35.0,
        help="wall seconds to wait after boot before F10 (default: 35)",
    )
    p.add_argument(
        "--duck-only",
        action="store_true",
        help="skip dw3-pc even if binary/runtime exist",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="print capture plan and exit 0; no disc/emulator/binary required",
    )
    return p


def log(msg: str) -> None:
    print(f"compare_harness: {msg}", file=sys.stderr)


def print_plan(args: argparse.Namespace) -> None:
    save = args.save if args.save is not None else Path("<unset>")
    out_dir = args.out / args.label
    log(f"stub/plan label={args.label} runtime={args.runtime}")
    log(f"  save (.mcr): {save}")
    log(f"  duckstation: {args.duckstation or '<unset>'}")
    log(f"  disc:        {args.disc or '<unset>'}")
    log(f"  bios:        {args.bios}")
    log(f"  pc-bin:      {args.pc_bin}")
    log(f"  settle:      {args.settle_seconds}s")
    log(f"  out:         {out_dir}/{{duck,pc}}.png + pair.md")


def check_bios(bios: Path) -> Path:
    if not bios.exists():
        raise FileNotFoundError(
            f"BIOS missing: {bios} (link scph1001.bin into "
            f"{DS_DATA / 'bios'} — do not download via this harness)"
        )
    target = bios.resolve()
    if not target.is_file() or target.stat().st_size < 100_000:
        raise FileNotFoundError(f"BIOS unreadable or too small: {target}")
    return target


def ensure_duckstation_config(screenshots_dir: Path) -> None:
    """Seed/patch DuckStation settings for unattended boot + F10 capture."""
    DS_DATA.mkdir(parents=True, exist_ok=True)
    (DS_DATA / "bios").mkdir(parents=True, exist_ok=True)
    screenshots_dir.mkdir(parents=True, exist_ok=True)

    settings = DS_DATA / "settings.ini"
    # Minimal INI: skip setup wizard, no power-off confirm, **100% speed**.
    # EmulationSpeed=1 is normal realtime. 0 means uncapped (thousands % —
    # unusable for P3.9). Cap FastForward/Turbo at 1 and leave FF unbound so
    # Tab cannot race the settle window. No CLI speed flag in DuckStation
    # 0.1-12074; speed is settings.ini only.
    text = f"""[Main]
SetupWizardIncomplete = false
SettingsVersion = 1
ConfirmPowerOff = false
SaveStateOnExit = false
StartFullscreen = false
EmulationSpeed = 1
FastForwardSpeed = 1
TurboSpeed = 1
SyncToHostRefreshRate = false
InhibitScreensaver = false
PauseOnFocusLoss = false

[BIOS]
SearchDirectory = bios
PathNTSCU = scph1001.bin

[Folders]
Screenshots = {screenshots_dir}

[Hotkeys]
Screenshot = Keyboard/F10
TogglePause = Keyboard/Space

[Logging]
LogLevel = Info
LogToConsole = true
LogToFile = true
"""
    settings.write_text(text, encoding="utf-8")
    log(f"wrote {settings} (EmulationSpeed=1 / 100%)")


def _duckstation_pids(duckstation: Path) -> list[int]:
    """PIDs whose argv[0] is the DuckStation binary (not shells mentioning the path)."""
    needles = {str(duckstation)}
    try:
        needles.add(str(duckstation.resolve()))
    except OSError:
        pass
    needles.add("DuckStation-x64.AppImage")
    found: list[int] = []
    for ent in Path("/proc").iterdir():
        if not ent.name.isdigit():
            continue
        pid = int(ent.name)
        if pid == os.getpid():
            continue
        try:
            raw = (ent / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        if not raw or not raw[0]:
            continue
        arg0 = raw[0].decode(errors="replace")
        if any(n in arg0 for n in needles) and "python" not in arg0:
            found.append(pid)
    return found


def kill_duckstation(duckstation: Path) -> None:
    for pid in _duckstation_pids(duckstation):
        try:
            os.kill(pid, signal.SIGTERM)
            log(f"SIGTERM pid={pid}")
        except OSError:
            pass
    time.sleep(1.0)
    for pid in _duckstation_pids(duckstation):
        try:
            os.kill(pid, signal.SIGKILL)
            log(f"SIGKILL pid={pid}")
        except OSError:
            pass


def wait_for_window(timeout: float = 60.0) -> str | None:
    """Return an X11 window id for the DuckStation game window."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            r = subprocess.run(
                ["xdotool", "search", "--class", "duckstation"],
                capture_output=True,
                text=True,
                check=False,
            )
            ids = [x for x in r.stdout.split() if x.isdigit()]
            best = None
            best_area = 0
            for wid in ids:
                name = subprocess.run(
                    ["xdotool", "getwindowname", wid],
                    capture_output=True,
                    text=True,
                    check=False,
                ).stdout.strip()
                # Skip tiny Qt helper windows / selection owners.
                if not name or name.startswith("Qt Selection"):
                    continue
                g = subprocess.run(
                    ["xdotool", "getwindowgeometry", "--shell", wid],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                vals: dict[str, int] = {}
                for line in g.stdout.splitlines():
                    if "=" in line:
                        k, v = line.split("=", 1)
                        if v.isdigit():
                            vals[k] = int(v)
                area = vals.get("WIDTH", 0) * vals.get("HEIGHT", 0)
                # Prefer the running-game window ("Digimon World 3") over the
                # main "DuckStation 0.1-…" UI chrome window.
                score = area
                if "Digimon" in name:
                    score += 10_000_000
                elif name.startswith("DuckStation"):
                    score += 100_000
                if score > best_area:
                    best_area = score
                    best = wid
            if best and best_area >= 200 * 200:
                return best
        except FileNotFoundError:
            log("xdotool missing; cannot focus window for F10")
            return None
        time.sleep(0.5)
    return None


def newest_png(directory: Path) -> Path | None:
    files = sorted(directory.glob("*.png"), key=lambda p: p.stat().st_mtime)
    return files[-1] if files else None


def capture_duckstation(
    *,
    duckstation: Path,
    disc: Path,
    bios: Path,
    out_png: Path,
    settle_seconds: float,
) -> Path:
    check_bios(bios)
    if not duckstation.exists():
        raise FileNotFoundError(f"DuckStation not found: {duckstation}")
    if not disc.is_file():
        raise FileNotFoundError(f"Disc image not found: {disc}")

    shots = DS_DATA / "screenshots_harness"
    ensure_duckstation_config(shots)
    # Clear prior harness shots so "newest" is unambiguous.
    for old in shots.glob("*.png"):
        old.unlink()

    kill_duckstation(duckstation)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    log_path = out_png.parent / "duckstation.log"

    cmd = [
        str(duckstation),
        "-batch",
        "-fastboot",
        "-nofullscreen",
        "-earlyconsole",
        "--",
        str(disc),
    ]
    log("launch: " + " ".join(cmd))
    with log_path.open("w", encoding="utf-8") as logf:
        proc = subprocess.Popen(
            cmd,
            stdout=logf,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )

    try:
        wid = wait_for_window(timeout=90.0)
        if not wid:
            raise RuntimeError(
                "DuckStation window not found (is DISPLAY set? need X11 + xdotool)"
            )
        log(f"window id={wid}; settling {settle_seconds:.0f}s at EmulationSpeed=1")
        time.sleep(settle_seconds)
        # Re-resolve after boot — game window title appears post-fastboot.
        wid = wait_for_window(timeout=15.0) or wid

        before = newest_png(shots)
        subprocess.run(
            ["xdotool", "windowactivate", "--sync", wid],
            check=False,
            capture_output=True,
        )
        time.sleep(0.3)
        subprocess.run(
            ["xdotool", "key", "--window", wid, "F10"],
            check=False,
            capture_output=True,
        )

        shot: Path | None = None
        for _ in range(40):
            time.sleep(0.25)
            cand = newest_png(shots)
            if cand is None:
                continue
            if before is None or cand != before or cand.stat().st_mtime > (
                before.stat().st_mtime if before.exists() else 0
            ):
                # Prefer a newly written file.
                if before is None or cand.name != before.name:
                    shot = cand
                    break
                if cand.stat().st_size > 0:
                    shot = cand
                    break

        if shot is None:
            # Fallback: ImageMagick import of the window (includes chrome).
            log("F10 screenshot missing; falling back to import -window")
            subprocess.run(
                ["import", "-window", wid, str(out_png)],
                check=True,
            )
            shot = out_png
        else:
            shutil.copy2(shot, out_png)
            log(f"copied native screenshot {shot.name} -> {out_png}")

        if not out_png.is_file() or out_png.stat().st_size < 1000:
            raise RuntimeError(f"capture failed or tiny file: {out_png}")
        return out_png
    finally:
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except OSError:
                proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except OSError:
                    proc.kill()
        kill_duckstation(duckstation)


def capture_dw3_pc(
    *,
    pc_bin: Path,
    runtime: Path,
    out_png: Path,
    settle_seconds: float,
) -> Path | None:
    if not pc_bin.is_file():
        log(f"skip pc: binary missing ({pc_bin})")
        return None
    if not runtime.is_dir():
        log(f"skip pc: runtime missing ({runtime})")
        return None

    out_png.parent.mkdir(parents=True, exist_ok=True)
    log_path = out_png.parent / "dw3-pc.log"
    cmd = [str(pc_bin.resolve()), "--runtime", str(runtime.resolve())]
    # Common host skip so we land nearer title if supported.
    env = os.environ.copy()
    env.setdefault("DW3_START_MODE", "title")
    log("launch pc: " + " ".join(cmd))
    with log_path.open("w", encoding="utf-8") as logf:
        proc = subprocess.Popen(
            cmd,
            stdout=logf,
            stderr=subprocess.STDOUT,
            env=env,
            start_new_session=True,
            cwd=str(pc_bin.resolve().parent.parent.parent),
        )
    try:
        time.sleep(max(5.0, settle_seconds * 0.5))
        # Best-effort: grab largest window matching dw3 / SDL.
        r = subprocess.run(
            ["xdotool", "search", "--name", "dw3"],
            capture_output=True,
            text=True,
            check=False,
        )
        ids = [x for x in r.stdout.split() if x.isdigit()]
        if not ids:
            r = subprocess.run(
                ["xdotool", "search", "--name", "SDL"],
                capture_output=True,
                text=True,
                check=False,
            )
            ids = [x for x in r.stdout.split() if x.isdigit()]
        if not ids:
            log("skip pc capture: no window found")
            return None
        wid = ids[-1]
        subprocess.run(["import", "-window", wid, str(out_png)], check=True)
        log(f"wrote {out_png}")
        return out_png
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        log(f"skip pc capture: {exc}")
        return None
    finally:
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except OSError:
                proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except OSError:
                    proc.kill()


def write_pair_md(
    path: Path,
    *,
    label: str,
    settle: float,
    duck: Path | None,
    pc: Path | None,
    notes: str,
) -> None:
    path.write_text(
        f"""# Pair review — {label}

- Date: {time.strftime('%Y-%m-%d')}
- Settle criterion: wall-clock {settle:.0f}s after DuckStation fastboot
  at EmulationSpeed=1 (100%%; FF/turbo capped). May still be opening
  movie, not settled MODE_TITLE, without pad skip / save state.
- DuckStation: `{duck if duck else 'missing'}`
- dw3-pc: `{pc if pc else 'skipped/missing'}`
- Verdict: pending human review
- Notes: {notes}
""",
        encoding="utf-8",
    )


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.dry_run:
        print_plan(args)
        return 0

    out_dir = args.out / args.label
    out_dir.mkdir(parents=True, exist_ok=True)
    duck_png = out_dir / "duck.png"
    pc_png = out_dir / "pc.png"

    if args.label in ("field", "battle") and args.save is None:
        log(
            f"label={args.label} needs --save (.mcr) or a -statefile for a "
            "settled scene; attempting boot capture anyway will not reach field/battle"
        )
        return 3

    if args.duckstation is None or args.disc is None:
        log("need --duckstation and --disc (or owner defaults on this machine)")
        return 2

    try:
        capture_duckstation(
            duckstation=args.duckstation,
            disc=args.disc,
            bios=args.bios,
            out_png=duck_png,
            settle_seconds=args.settle_seconds,
        )
    except Exception as exc:
        log(f"duck capture failed: {exc}")
        return 1

    pc_path: Path | None = None
    if not args.duck_only:
        pc_path = capture_dw3_pc(
            pc_bin=args.pc_bin,
            runtime=args.runtime,
            out_png=pc_png,
            settle_seconds=args.settle_seconds,
        )

    notes = (
        "Automated boot/title-window capture. Field/battle still blocked "
        "without shared .mcr + pad or DuckStation -statefile. "
        "PNG kept under gitignored build/compare/."
    )
    write_pair_md(
        out_dir / "pair.md",
        label=args.label,
        settle=args.settle_seconds,
        duck=duck_png if duck_png.is_file() else None,
        pc=pc_path,
        notes=notes,
    )
    log(f"done label={args.label} duck={duck_png.is_file()} pc={pc_path is not None}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
