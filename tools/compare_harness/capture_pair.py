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
            "frames for P3.9. Speed forced to EmulationSpeed=2 (200%) via settings.ini. "
            "0 is uncapped; do not use it."
        )
    )
    p.add_argument("--label", required=True, choices=LABELS)
    p.add_argument("--runtime", type=Path, default=Path("runtime/us"))
    p.add_argument(
        "--save",
        type=Path,
        default=None,
        help="128 KiB slot-0 .mcr/.mcd (required for field, and for duck battle)",
    )
    p.add_argument(
        "--statefile",
        type=Path,
        default=None,
        help="DuckStation -statefile for the oracle only (does not boot dw3-pc)",
    )
    p.add_argument(
        "--encounter",
        type=int,
        default=1,
        help="DW3_FORCE_ENCOUNTER index for the battle PC subject (default 1)",
    )
    p.add_argument(
        "--pc-settle-seconds",
        type=float,
        default=None,
        help="extra idle after FIGHTSTG loads WFIGHTMN, before the PC grab",
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
    p.add_argument(
        "--pc-only",
        action="store_true",
        help="capture dw3-pc only; battle uses FORCE_ENCOUNTER, not -statefile",
    )
    p.add_argument("--dry-run", action="store_true")
    return p


def log(msg: str) -> None:
    print(f"compare_harness: {msg}", file=sys.stderr)


def default_settle(label: str) -> float:
    return 25.0 if label == "title" else 150.0


# Idle after "FIGHTSTG loaded WFIGHTMN" so the intro can reach the command menu.
# DW3_FORCE_WIN is never set; that path skips the menu.
PC_BATTLE_MENU_SETTLE = 22.0
PC_BATTLE_LOG_TIMEOUT = 150.0


def print_plan(args: argparse.Namespace) -> None:
    settle = args.settle_seconds or default_settle(args.label)
    log(f"plan label={args.label} settle={settle}s")
    log(f"  save:        {args.save or '<unset>'}")
    log(f"  statefile:   {args.statefile or '<unset>'} (duck only)")
    log(f"  duckstation: {args.duckstation or '<unset>'}")
    log(f"  disc:        {args.disc or '<unset>'}")
    log(f"  duck-only:   {args.duck_only}  pc-only: {args.pc_only}")
    if args.label == "battle" and not args.duck_only:
        extra = (
            args.pc_settle_seconds
            if args.pc_settle_seconds is not None
            else PC_BATTLE_MENU_SETTLE
        )
        log(
            f"  pc battle:   DW3_START_MODE=0x600 DW3_FORCE_ENCOUNTER={args.encounter} "
            f"menu-settle={extra:.0f}s (no DW3_FORCE_WIN)"
        )
    log(f"  out:         {args.out / args.label}/")


def validate_capture(args: argparse.Namespace) -> int | None:
    """Return an exit code when the plan cannot run, else None."""
    if args.duck_only and args.pc_only:
        log("pass only one of --duck-only and --pc-only")
        return 2
    want_duck = not args.pc_only
    want_pc = not args.duck_only
    if (
        want_duck
        and args.label in ("field", "battle")
        and args.save is None
        and args.statefile is None
    ):
        log(f"label={args.label} duck capture needs --save and/or --statefile")
        return 3
    if want_pc and args.label == "field" and args.save is None:
        log("label=field PC capture needs --save")
        return 3
    if want_duck and (args.duckstation is None or args.disc is None):
        log("need --duckstation and --disc")
        return 2
    if want_pc and args.label == "battle" and args.encounter < 0:
        log("--encounter must be >= 0")
        return 2
    return None


def pc_subject_env(base: dict[str, str], label: str, encounter: int) -> dict[str, str]:
    """Env for dw3-pc. Battle boots FIGHTSTG; the DuckStation savestate is not applied.

    DW3_FORCE_WIN is removed so the command menu is not skipped.
    """
    env = dict(base)
    env.pop("DW3_AUTO_PAD", None)
    env.pop("DW3_FORCE_MCARD_LOAD", None)
    env.pop("DW3_PROGRESS", None)
    env.pop("DW3_FORCE_WIN", None)
    env.pop("DW3_FORCE_ENCOUNTER", None)
    env.pop("DW3_START_MODE", None)
    if label == "battle":
        env["DW3_START_MODE"] = "0x600"
        env["DW3_FORCE_ENCOUNTER"] = str(encounter)
    return env


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
    # 1 = 100%, 2 = 200%, 0 = uncapped. Never write 0.
    cp["Main"]["EmulationSpeed"] = "2"
    cp["Main"]["FastForwardSpeed"] = "2"
    cp["Main"]["TurboSpeed"] = "2"
    cp["Main"]["SyncToHostRefreshRate"] = "false"
    cp["Main"]["InhibitScreensaver"] = "false"
    cp["Main"]["PauseOnFocusLoss"] = "false"

    cp["BIOS"]["SearchDirectory"] = "bios"
    cp["BIOS"]["PathNTSCU"] = "scph1001.bin"

    screenshots_dir.mkdir(parents=True, exist_ok=True)
    cp["Folders"]["Screenshots"] = str(screenshots_dir)

    cp["Hotkeys"]["Screenshot"] = "Keyboard/F10"
    # Do not bind FastForward. FastForwardSpeed=2 caps a stray toggle at 200%.
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
    log(f"patched {settings} (EmulationSpeed=2, 200%)")
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
            ["xdotool", "keydown", "--window", wid, key],
            check=False,
            capture_output=True,
        )
        time.sleep(0.12)
        subprocess.run(
            ["xdotool", "keyup", "--window", wid, key],
            check=False,
            capture_output=True,
        )
        time.sleep(pause)


def _wid(wid: str) -> str:
    return wait_for_window(timeout=5.0) or wid


def settle_title_press_start(wid: str) -> str:
    """One Start skips the opening FMV. A second Start opens New Game / Continue.

    MODE_TITLE returns to the movie after 600 frames with no Start.
    At EmulationSpeed=2 that window is about 5s of wall clock once PRESS
    START is up, so the idle after the skip stays inside it.
    """
    log("title: wait 8s for opening movie @ EmulationSpeed=2")
    time.sleep(8.0)
    wid = _wid(wid)
    log("title: Start once (skip FMV -> MODE_TITLE)")
    send_keys(wid, "Return", pause=0.2)
    log("title: idle 5s for logo / PRESS START")
    time.sleep(5.0)
    return _wid(wid)


def settle_field_continue(wid: str, *, start_key: str, cross_key: str) -> str:
    """Skip FMV, Start, Cross on Continue (default cursor), then stop.

    Extra Start after the load opens the field menu. This sequence does not
    press Start again after Continue.
    """
    log("field: wait 8s for opening movie @ EmulationSpeed=2")
    time.sleep(8.0)
    wid = _wid(wid)
    log("field: Start once to skip FMV")
    send_keys(wid, start_key, pause=0.2)
    log("field: idle 4s on PRESS START")
    time.sleep(4.0)
    wid = _wid(wid)
    log("field: Start to show New Game / Continue")
    send_keys(wid, start_key, pause=0.3)
    time.sleep(1.5)
    wid = _wid(wid)
    log("field: Cross (Continue is the default cursor)")
    send_keys(wid, cross_key, pause=0.3)
    # Slot list, then the "Loaded." card screen. A later Cross leaves it.
    # Cross only — Start after the load opens the field menu.
    log("field: Cross pulses while the card reads (10s)")
    deadline = time.time() + 10.0
    while time.time() < deadline:
        wid = _wid(wid)
        send_keys(wid, cross_key, pause=0.2)
        time.sleep(1.6)
    # "Loaded." waits on a Cross edge (STGMCARD_waitLoadedMessage). One tap
    # often lands before prompting=1. A few edges, then stop so the field
    # fade is not mashed into a menu.
    log("field: idle 3s on Loaded, then Cross edges")
    time.sleep(3.0)
    for _ in range(3):
        wid = _wid(wid)
        send_keys(wid, cross_key, pause=0.4)
        time.sleep(1.8)
    log("field: idle 8s for the field fade-in")
    time.sleep(8.0)
    return _wid(wid)


def auto_load_pad(wid: str, settle_seconds: float) -> None:
    """Legacy spam. Field/title use the sequenced settles instead."""
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
    label: str,
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
    # Title skip and field Continue both need the temporary keyboard pad.
    use_pad = statefile is None and (label == "title" or auto_load)
    prev_pad = patch_duckstation_settings(shots, keyboard_pad=use_pad)
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
            if statefile is not None or not use_pad:
                log(f"settling {settle_seconds:.0f}s at EmulationSpeed=2")
                time.sleep(settle_seconds)
            elif label == "title":
                wid = settle_title_press_start(wid)
            elif label == "field" and auto_load:
                wid = settle_field_continue(wid, start_key="Return", cross_key="k")
            elif auto_load:
                auto_load_pad(wid, settle_seconds)
            else:
                log(f"settling {settle_seconds:.0f}s at EmulationSpeed=2")
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


def wait_pc_window(timeout: float = 30.0) -> str | None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        r = subprocess.run(
            ["xdotool", "search", "--name", "dw3-pc"],
            capture_output=True,
            text=True,
            check=False,
        )
        ids = [x for x in r.stdout.split() if x.isdigit()]
        if ids:
            return ids[-1]
        time.sleep(0.4)
    return None


def grab_pc_window(wid: str, out_png: Path) -> None:
    subprocess.run(["import", "-window", wid, str(out_png)], check=True)


def log_contains(path: Path, needle: str) -> bool:
    try:
        data = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False
    return needle in data


def wait_for_log(
    path: Path, needle: str, timeout: float, proc: subprocess.Popen
) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"pc exited {proc.returncode} before {needle!r}")
            return False
        if log_contains(path, needle):
            return True
        time.sleep(0.4)
    return False


def capture_dw3_pc(
    *,
    label: str,
    pc_bin: Path,
    runtime: Path,
    out_png: Path,
    memcard_dir: Path | None,
    encounter: int = 1,
    pc_settle_seconds: float | None = None,
) -> Path | None:
    """Subject frame.

    Title/field leave DW3_START_MODE unset so patch 0010 stays MODE_TITLE.
    Battle sets DW3_START_MODE=0x600 and DW3_FORCE_ENCOUNTER. It does not
    load a DuckStation savestate and does not set DW3_FORCE_WIN.

    PSY-Z keyboard: Return=Start, x=Cross, s=Triangle.
    """
    if not pc_bin.is_file():
        log(f"skip pc: binary missing ({pc_bin})")
        return None
    if not runtime.is_dir():
        log(f"skip pc: runtime missing ({runtime})")
        return None
    out_png.parent.mkdir(parents=True, exist_ok=True)
    log_path = out_png.parent / "dw3-pc.log"
    cmd = [str(pc_bin.resolve()), "--runtime", str(runtime.resolve())]
    if memcard_dir is not None:
        cmd += ["--memcard-dir", str(memcard_dir.resolve())]
    env = pc_subject_env(os.environ, label, encounter)
    if label == "battle":
        log(
            "pc battle env: DW3_START_MODE=0x600 "
            f"DW3_FORCE_ENCOUNTER={encounter} DW3_FORCE_WIN unset"
        )
    launch = cmd
    stdbuf = shutil.which("stdbuf")
    if stdbuf:
        # Line-buffer so the field settle can see mode=0x20a before the next Cross.
        launch = [stdbuf, "-oL", "-eL", *cmd]
    log("launch pc: " + " ".join(launch))
    with log_path.open("w", encoding="utf-8") as logf:
        proc = subprocess.Popen(
            launch,
            stdout=logf,
            stderr=subprocess.STDOUT,
            env=env,
            start_new_session=True,
            cwd=str(pc_bin.resolve().parent.parent.parent),
        )
    try:
        wid = wait_pc_window(timeout=40.0)
        if not wid:
            log("skip pc capture: no window")
            return None
        log(f"pc window id={wid}")
        if label == "title":
            # Logo + PRESS START. The 600-frame timeout leaves MODE_TITLE.
            log("pc title: idle 6s on MODE_TITLE")
            time.sleep(6.0)
        elif label == "field":
            log("pc field: idle 5s then Start / Cross Continue")
            time.sleep(5.0)
            wid = wait_pc_window(timeout=5.0) or wid
            send_keys(wid, "Return", pause=0.3)
            time.sleep(2.0)
            wid = wait_pc_window(timeout=5.0) or wid
            send_keys(wid, "x", pause=0.3)
            # Cross leaves "Loaded." The next Cross on the inn floor hits
            # talkToActorAt and opens "Save here?". Stop when field mode is up.
            log("pc field: Cross until mode=0x20a, then stop")
            deadline = time.time() + 28.0
            entered = False
            while time.time() < deadline:
                if log_contains(log_path, "mode=0x20a"):
                    entered = True
                    break
                wid = wait_pc_window(timeout=3.0) or wid
                send_keys(wid, "x", pause=0.2)
                time.sleep(1.2)
            if entered:
                log("pc field: field mode up; idle 10s with no further Cross")
            else:
                log("pc field: mode=0x20a not seen; idle 10s anyway")
            time.sleep(10.0)
            wid = wait_pc_window(timeout=5.0) or wid
        elif label == "battle":
            # WFIGHTMN load is the menu overlay, not yet the command list.
            # Extra idle covers the intro so the grab is a fight frame.
            # Do not press Cross and do not set DW3_FORCE_WIN.
            menu_settle = (
                pc_settle_seconds
                if pc_settle_seconds is not None
                else PC_BATTLE_MENU_SETTLE
            )
            loaded = wait_for_log(
                log_path,
                "FIGHTSTG loaded WFIGHTMN",
                PC_BATTLE_LOG_TIMEOUT,
                proc,
            )
            if log_contains(log_path, "DW3_FORCE_WIN queued"):
                log("pc battle: FORCE_WIN fired; command menu was skipped")
            if loaded:
                log(f"pc battle: WFIGHTMN loaded; idle {menu_settle:.0f}s")
            else:
                log(
                    "pc battle: WFIGHTMN log not seen; "
                    f"idle {menu_settle:.0f}s anyway"
                )
            time.sleep(menu_settle)
            wid = wait_pc_window(timeout=5.0) or wid
            subprocess.run(
                ["xdotool", "windowactivate", "--sync", wid],
                check=False,
                capture_output=True,
            )
        else:
            time.sleep(5.0)
        wid = wait_pc_window(timeout=5.0) or wid
        grab_pc_window(wid, out_png)
        if not out_png.is_file() or out_png.stat().st_size < 1000:
            log(f"pc capture tiny or missing: {out_png}")
            return None
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
    settle_text: str,
    duck: Path | None,
    pc: Path | None,
    save: Path | None,
    notes: str,
) -> None:
    path.write_text(
        f"""# Pair review — {label}

- Date: {time.strftime('%Y-%m-%d')}
- Settle: {settle_text}
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

    blocked = validate_capture(args)
    if blocked is not None:
        return blocked

    want_duck = not args.pc_only
    want_pc = not args.duck_only
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

    staged: Path | None = None
    pc_memdir: Path | None = None
    if args.save is not None and want_duck:
        try:
            staged = stage_memcard(args.save, harness_card)
        except (OSError, ValueError) as exc:
            log(f"stage save failed: {exc}")
            return 1
        if args.label == "field":
            pc_memdir = args.out / "pc-memcard"
            pc_memdir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(staged, pc_memdir / "memcard0.mcr")
            log(f"pc card: {pc_memdir / 'memcard0.mcr'}")

    if want_duck:
        try:
            capture_duckstation(
                label=args.label,
                duckstation=args.duckstation,
                disc=args.disc,
                bios=args.bios,
                out_png=duck_png,
                settle_seconds=settle,
                statefile=args.statefile,
                auto_load=auto_load,
            )
        except Exception as exc:
            log(f"duck capture failed: {exc}")
            return 1

    pc_path: Path | None = None
    if want_pc:
        pc_path = capture_dw3_pc(
            label=args.label,
            pc_bin=args.pc_bin,
            runtime=args.runtime,
            out_png=pc_png,
            memcard_dir=pc_memdir,
            encounter=args.encounter,
            pc_settle_seconds=args.pc_settle_seconds,
        )

    if want_pc and not want_duck and args.label == "battle":
        shown = (
            args.pc_settle_seconds
            if args.pc_settle_seconds is not None
            else PC_BATTLE_MENU_SETTLE
        )
        settle_text = (
            f"{shown:.0f}s after FIGHTSTG loaded WFIGHTMN (DW3_FORCE_WIN unset)"
        )
    else:
        settle_text = f"{settle:.0f}s wall @ EmulationSpeed=2 (200%)"

    if args.label == "battle" and want_pc:
        battle_note = (
            f"pc FIGHTSTG via DW3_START_MODE=0x600 "
            f"DW3_FORCE_ENCOUNTER={args.encounter}; no DW3_FORCE_WIN; "
            "savestate is not loaded into dw3-pc. "
        )
    elif args.label == "battle":
        battle_note = "duck-only; savestate is not applied to dw3-pc. "
    else:
        battle_note = ""
    notes = (
        f"label={args.label}; card={staged}; statefile={args.statefile}; "
        f"auto_load={auto_load}. {battle_note}"
        "PNGs gitignored under build/compare/."
    )
    write_pair_md(
        out_dir / "pair.md",
        label=args.label,
        settle_text=settle_text,
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
