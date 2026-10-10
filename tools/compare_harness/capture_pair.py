#!/usr/bin/env python3
"""Capture DuckStation vs dw3-pc frames for the same save/label (P3.9)."""

from __future__ import annotations

import argparse
import configparser
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
# Per-game slot-0 card DuckStation already uses for US DW3.
DS_GAME_CARD = DS_DATA / "memcards" / "Digimon World 3 (USA)_1.mcd"

# Temporary keyboard pad for unattended Start/Cross (restored after run).
# Names must match DuckStation InputManager (Keyboard/Z is invalid; use K/Enter).
KEYBOARD_PAD1 = {
    "Up": "Keyboard/UpArrow",
    "Down": "Keyboard/DownArrow",
    "Left": "Keyboard/LeftArrow",
    "Right": "Keyboard/RightArrow",
    "Cross": "Keyboard/K",
    "Circle": "Keyboard/L",
    "Square": "Keyboard/J",
    "Triangle": "Keyboard/I",
    "Start": "Keyboard/Enter",
    "Select": "Keyboard/Backspace",
}


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=(
            "Capture DuckStation (oracle) and optionally dw3-pc (subject) "
            "frames for P3.9. Speed forced to EmulationSpeed=1 via settings.ini."
        )
    )
    p.add_argument("--label", required=True, choices=LABELS)
    p.add_argument("--runtime", type=Path, default=Path("runtime/us"))
    p.add_argument(
        "--save",
        type=Path,
        default=None,
        help="128 KiB slot-0 .mcr/.mcd (required for field/battle)",
    )
    p.add_argument(
        "--statefile",
        type=Path,
        default=None,
        help="optional DuckStation -statefile (best for settled field/battle)",
    )
    p.add_argument(
        "--duckstation",
        type=Path,
        default=DEFAULT_DUCKSTATION if DEFAULT_DUCKSTATION.exists() else None,
    )
    p.add_argument(
        "--disc",
        type=Path,
        default=DEFAULT_DISC if DEFAULT_DISC.exists() else None,
    )
    p.add_argument("--bios", type=Path, default=DEFAULT_BIOS)
    p.add_argument("--pc-bin", type=Path, default=Path("build/pc/dw3-pc"))
    p.add_argument("--out", type=Path, default=Path("build/compare"))
    p.add_argument(
        "--settle-seconds",
        type=float,
        default=None,
        help="override settle wait (defaults: title=25, field/battle=150)",
    )
    p.add_argument(
        "--auto-load",
        action="store_true",
        default=True,
        help="spam Start/Cross during settle to skip movies + load (default on)",
    )
    p.add_argument(
        "--no-auto-load",
        action="store_true",
        help="disable pad spam (use with --statefile)",
    )
    p.add_argument("--duck-only", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    return p


def log(msg: str) -> None:
    print(f"compare_harness: {msg}", file=sys.stderr)


def default_settle(label: str) -> float:
    return 25.0 if label == "title" else 150.0


def print_plan(args: argparse.Namespace) -> None:
    settle = args.settle_seconds or default_settle(args.label)
    log(f"plan label={args.label} settle={settle}s")
    log(f"  save:        {args.save or '<unset>'}")
    log(f"  statefile:   {args.statefile or '<unset>'}")
    log(f"  duckstation: {args.duckstation or '<unset>'}")
    log(f"  disc:        {args.disc or '<unset>'}")
    log(f"  out:         {args.out / args.label}/")


def check_bios(bios: Path) -> Path:
    if not bios.exists():
        raise FileNotFoundError(f"BIOS missing: {bios}")
    target = bios.resolve()
    if not target.is_file() or target.stat().st_size < 100_000:
        raise FileNotFoundError(f"BIOS unreadable or too small: {target}")
    return target


def _read_ini(path: Path) -> configparser.ConfigParser:
    cp = configparser.ConfigParser()
    cp.optionxform = str  # preserve key case
    if path.is_file():
        cp.read(path, encoding="utf-8")
    return cp


def _write_ini(path: Path, cp: configparser.ConfigParser) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        cp.write(f, space_around_delimiters=True)


def patch_duckstation_settings(
    screenshots_dir: Path, *, keyboard_pad: bool
) -> dict[str, str] | None:
    """Merge harness keys into settings.ini. Returns previous Pad1 map if replaced."""
    settings = DS_DATA / "settings.ini"
    cp = _read_ini(settings)
    for sec in ("Main", "BIOS", "Folders", "Hotkeys", "Logging"):
        if not cp.has_section(sec):
            cp.add_section(sec)

    cp["Main"]["SetupWizardIncomplete"] = "false"
    cp["Main"]["SettingsVersion"] = cp["Main"].get("SettingsVersion", "1")
    cp["Main"]["ConfirmPowerOff"] = "false"
    cp["Main"]["SaveStateOnExit"] = "false"
    cp["Main"]["StartFullscreen"] = "false"
    cp["Main"]["EmulationSpeed"] = "1"
    cp["Main"]["FastForwardSpeed"] = "1"
    cp["Main"]["TurboSpeed"] = "1"
    cp["Main"]["SyncToHostRefreshRate"] = "false"
    cp["Main"]["InhibitScreensaver"] = "false"
    cp["Main"]["PauseOnFocusLoss"] = "false"

    cp["BIOS"]["SearchDirectory"] = "bios"
    cp["BIOS"]["PathNTSCU"] = "scph1001.bin"

    screenshots_dir.mkdir(parents=True, exist_ok=True)
    cp["Folders"]["Screenshots"] = str(screenshots_dir)

    cp["Hotkeys"]["Screenshot"] = "Keyboard/F10"
    # Do not bind FastForward — keep settle at 100%.
    if cp.has_option("Hotkeys", "FastForward"):
        cp.remove_option("Hotkeys", "FastForward")
    if cp.has_option("Hotkeys", "ToggleFastForward"):
        cp.remove_option("Hotkeys", "ToggleFastForward")

    cp["Logging"]["LogLevel"] = "Info"
    cp["Logging"]["LogToConsole"] = "true"
    cp["Logging"]["LogToFile"] = "true"

    prev_pad: dict[str, str] | None = None
    if keyboard_pad:
        if not cp.has_section("Pad1"):
            cp.add_section("Pad1")
        prev_pad = dict(cp["Pad1"])
        # Replace with keyboard for unattended Start/Cross; restored after run.
        for k in list(cp["Pad1"].keys()):
            cp.remove_option("Pad1", k)
        for k, v in KEYBOARD_PAD1.items():
            cp["Pad1"][k] = v
        log("Pad1 temporarily set to keyboard (Start=Enter, Cross=K)")

    _write_ini(settings, cp)
    log(f"patched {settings} (EmulationSpeed=1)")
    return prev_pad


def restore_pad1(prev: dict[str, str] | None) -> None:
    if prev is None:
        return
    settings = DS_DATA / "settings.ini"
    cp = _read_ini(settings)
    if not cp.has_section("Pad1"):
        cp.add_section("Pad1")
    for k in list(cp["Pad1"].keys()):
        cp.remove_option("Pad1", k)
    for k, v in prev.items():
        cp["Pad1"][k] = v
    _write_ini(settings, cp)
    log("restored previous Pad1 bindings")


def stage_memcard(save: Path, harness_card: Path) -> Path:
    """Copy save to gitignored harness path + DuckStation per-game slot-0."""
    if not save.is_file():
        raise FileNotFoundError(f"--save not found: {save}")
    size = save.stat().st_size
    if size != 131072:
        raise ValueError(f"--save must be 128 KiB raw card, got {size} bytes: {save}")

    harness_card.parent.mkdir(parents=True, exist_ok=True)
    save_res = save.resolve()
    harness_res = harness_card.resolve() if harness_card.exists() else harness_card
    # Never unlink the source when --save already is the harness path.
    if save_res != Path(harness_res):
        if harness_card.exists() or harness_card.is_symlink():
            harness_card.unlink()
        shutil.copy2(save, harness_card)
    elif not harness_card.is_file():
        shutil.copy2(save, harness_card)

    DS_GAME_CARD.parent.mkdir(parents=True, exist_ok=True)
    # Backup existing DS card once if different from what we will install.
    if (
        DS_GAME_CARD.is_file()
        and DS_GAME_CARD.resolve() != harness_card.resolve()
        and DS_GAME_CARD.resolve() != save_res
    ):
        bak = DS_GAME_CARD.with_suffix(".mcd.bak-harness")
        if not bak.exists():
            shutil.copy2(DS_GAME_CARD, bak)
            log(f"backed up DS card -> {bak}")
    shutil.copy2(harness_card if harness_card.is_file() else save, DS_GAME_CARD)
    log(f"staged card: {save} -> {harness_card} + {DS_GAME_CARD}")
    return harness_card


def _duckstation_pids(duckstation: Path) -> list[int]:
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
    """Prefer the Digimon game window; never return huge desktop/chrome ids."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            # Prefer exact game title via wmctrl when available.
            wmc = subprocess.run(
                ["wmctrl", "-lx"],
                capture_output=True,
                text=True,
                check=False,
            )
            for line in wmc.stdout.splitlines():
                if "duckstation" not in line.lower():
                    continue
                if "Digimon" in line:
                    wid_hex = line.split()[0]
                    try:
                        return str(int(wid_hex, 16))
                    except ValueError:
                        pass

            r = subprocess.run(
                ["xdotool", "search", "--class", "duckstation"],
                capture_output=True,
                text=True,
                check=False,
            )
            ids = [x for x in r.stdout.split() if x.isdigit()]
            best = None
            best_score = 0
            for wid in ids:
                name = subprocess.run(
                    ["xdotool", "getwindowname", wid],
                    capture_output=True,
                    text=True,
                    check=False,
                ).stdout.strip()
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
                w, h = vals.get("WIDTH", 0), vals.get("HEIGHT", 0)
                area = w * h
                # Reject full-desktop grabs (typical multi-monitor > 2M px).
                if area <= 0 or area > 2_000_000 or w > 1600:
                    continue
                score = area
                if "Digimon" in name:
                    score += 10_000_000
                elif name.startswith("DuckStation"):
                    score += 100_000
                if score > best_score:
                    best_score = score
                    best = wid
            if best and best_score >= 200 * 200:
                return best
        except FileNotFoundError:
            log("xdotool/wmctrl missing")
            return None
        time.sleep(0.5)
    return None


def send_keys(wid: str, *keys: str, pause: float = 0.35) -> None:
    subprocess.run(
        ["xdotool", "windowactivate", "--sync", wid],
        check=False,
        capture_output=True,
    )
    for key in keys:
        subprocess.run(
            ["xdotool", "key", "--window", wid, key],
            check=False,
            capture_output=True,
        )
        time.sleep(pause)


def auto_load_pad(wid: str, settle_seconds: float) -> None:
    """Best-effort: skip movies (Start/Enter) then Cross(K) toward Continue/Load."""
    log(f"auto-load pad for {settle_seconds:.0f}s (Enter=Start, k=Cross)")
    deadline = time.time() + settle_seconds
    phase_menu_at = time.time() + min(90.0, settle_seconds * 0.55)
    while time.time() < deadline:
        w2 = wait_for_window(timeout=2.0)
        if not w2:
            log("auto-load: game window lost; aborting pad spam")
            break
        wid = w2
        send_keys(wid, "Return", pause=0.15)
        time.sleep(1.2)
        if time.time() >= phase_menu_at:
            send_keys(wid, "Return", "k", "k", pause=0.4)
            time.sleep(1.5)


def newest_png(directory: Path) -> Path | None:
    files = sorted(directory.glob("*.png"), key=lambda p: p.stat().st_mtime)
    return files[-1] if files else None


def take_screenshot(wid: str, shots: Path, out_png: Path) -> Path:
    # Window ids can go stale after long auto-load; re-resolve.
    wid = wait_for_window(timeout=10.0) or wid
    before = newest_png(shots)
    send_keys(wid, "F10", pause=0.2)
    shot: Path | None = None
    for _ in range(40):
        time.sleep(0.25)
        cand = newest_png(shots)
        if cand is None:
            continue
        if before is None or cand.name != before.name:
            shot = cand
            break
    if shot is None:
        wid = wait_for_window(timeout=5.0) or wid
        log(f"F10 missing; import -window {wid}")
        subprocess.run(["import", "-window", wid, str(out_png)], check=True)
        return out_png
    shutil.copy2(shot, out_png)
    log(f"copied {shot.name} -> {out_png}")
    return out_png


def capture_duckstation(
    *,
    duckstation: Path,
    disc: Path,
    bios: Path,
    out_png: Path,
    settle_seconds: float,
    statefile: Path | None,
    auto_load: bool,
) -> Path:
    check_bios(bios)
    if not duckstation.exists():
        raise FileNotFoundError(f"DuckStation not found: {duckstation}")
    if not disc.is_file():
        raise FileNotFoundError(f"Disc image not found: {disc}")
    if statefile is not None and not statefile.is_file():
        raise FileNotFoundError(f"--statefile not found: {statefile}")

    shots = DS_DATA / "screenshots_harness"
    prev_pad = patch_duckstation_settings(shots, keyboard_pad=auto_load)
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
    ]
    if statefile is not None:
        cmd += ["-statefile", str(statefile)]
    cmd += ["--", str(disc)]

    log("launch: " + " ".join(cmd))
    try:
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
                raise RuntimeError("DuckStation window not found (DISPLAY/xdotool?)")
            log(f"window id={wid}")
            if auto_load and statefile is None:
                auto_load_pad(wid, settle_seconds)
            else:
                log(f"settling {settle_seconds:.0f}s at EmulationSpeed=1")
                time.sleep(settle_seconds)
            wid = wait_for_window(timeout=15.0) or wid
            take_screenshot(wid, shots, out_png)
            if not out_png.is_file() or out_png.stat().st_size < 1000:
                raise RuntimeError(f"capture failed or tiny: {out_png}")
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
    finally:
        restore_pad1(prev_pad)


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
        time.sleep(max(5.0, settle_seconds * 0.25))
        r = subprocess.run(
            ["xdotool", "search", "--name", "dw3"],
            capture_output=True,
            text=True,
            check=False,
        )
        ids = [x for x in r.stdout.split() if x.isdigit()]
        if not ids:
            log("skip pc capture: no window")
            return None
        subprocess.run(["import", "-window", ids[-1], str(out_png)], check=True)
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
    save: Path | None,
    notes: str,
) -> None:
    path.write_text(
        f"""# Pair review — {label}

- Date: {time.strftime('%Y-%m-%d')}
- Settle: {settle:.0f}s @ EmulationSpeed=1
- Save: `{save if save else 'none'}`
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

    settle = (
        args.settle_seconds
        if args.settle_seconds is not None
        else default_settle(args.label)
    )
    auto_load = args.auto_load and not args.no_auto_load
    if args.statefile is not None:
        auto_load = False

    out_dir = args.out / args.label
    out_dir.mkdir(parents=True, exist_ok=True)
    duck_png = out_dir / "duck.png"
    pc_png = out_dir / "pc.png"
    harness_card = args.out / "memcard0.mcr"

    if args.label in ("field", "battle") and args.save is None and args.statefile is None:
        log(f"label={args.label} needs --save and/or --statefile")
        return 3

    if args.duckstation is None or args.disc is None:
        log("need --duckstation and --disc")
        return 2

    staged: Path | None = None
    if args.save is not None:
        try:
            staged = stage_memcard(args.save, harness_card)
        except (OSError, ValueError) as exc:
            log(f"stage save failed: {exc}")
            return 1

    try:
        capture_duckstation(
            duckstation=args.duckstation,
            disc=args.disc,
            bios=args.bios,
            out_png=duck_png,
            settle_seconds=settle,
            statefile=args.statefile,
            auto_load=auto_load and args.label != "title",
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
            settle_seconds=settle,
        )

    notes = (
        f"label={args.label}; card={staged}; statefile={args.statefile}; "
        f"auto_load={auto_load}. PNGs gitignored under build/compare/."
    )
    write_pair_md(
        out_dir / "pair.md",
        label=args.label,
        settle=settle,
        duck=duck_png if duck_png.is_file() else None,
        pc=pc_path,
        save=staged,
        notes=notes,
    )
    log(f"done label={args.label} duck={duck_png.is_file()} bytes="
        f"{duck_png.stat().st_size if duck_png.is_file() else 0}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
