#!/usr/bin/env python3
"""P3.8 US stage sweep: inventory FIELDSTG_stages vs linked host stages,
optionally boot STAGSLCT → stage → return for linked modes.

  tools/stage_sweep.py inventory
  tools/stage_sweep.py run [--only 0x2d7,0x2d8,0x2d9]
  tools/stage_sweep.py write-results
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDSTG = ROOT / "vendor/dw3_decomp/src/field/field_mode/data/fieldstg.c"
LINKED = {
    "WSTAG260",
    "WSTAG780",
    "WSTAG785",
    "WSTAG790",
    # P3.8 batch 1 — early-game / connected from 780/785/790
    "WSTAG200",
    "WSTAG201",
    "WSTAG206",
    "WSTAG210",
    "WSTAG212",
    "WSTAG220",
    "WSTAG225",
    "WSTAG230",
    "WSTAG235",
    "WSTAG240",
    "WSTAG245",
    "WSTAG250",
    "WSTAG255",
    # P3.8 batch 2 — remaining early Central/Asuka + Amaterasu twins
    "WSTAG202",
    "WSTAG203",
    "WSTAG205",
    "WSTAG218",
    "WSTAG219",
    "WSTAG232",
    "WSTAG237",
    "WSTAG270",
    "WSTAG275",
    "WSTAG280",
    "WSTAG285",
    "WSTAG290",
    "WSTAG295",
    "WSTAG300",
    "WSTAG305",
    "WSTAG310",
    "WSTAG315",
    "WSTAG320",
    "WSTAG325",
    "WSTAG211",
    "WSTAG221",
    "WSTAG226",
    "WSTAG231",
    "WSTAG233",
    "WSTAG236",
    "WSTAG238",
    "WSTAG241",
    "WSTAG246",
    "WSTAG251",
    "WSTAG256",
    "WSTAG261",
    # P3.8 batch 3 — Amaterasu twins of 270-325 + Central/East/Seiryu/South
    "WSTAG271",
    "WSTAG276",
    "WSTAG281",
    "WSTAG286",
    "WSTAG291",
    "WSTAG296",
    "WSTAG301",
    "WSTAG306",
    "WSTAG311",
    "WSTAG316",
    "WSTAG321",
    "WSTAG326",
    "WSTAG330",
    "WSTAG335",
    "WSTAG340",
    "WSTAG345",
    "WSTAG350",
    "WSTAG355",
    "WSTAG360",
    "WSTAG365",
    "WSTAG370",
    "WSTAG375",
    "WSTAG380",
    "WSTAG385",
    "WSTAG395",
    "WSTAG400",
    "WSTAG405",
    "WSTAG410",
    "WSTAG415",
    "WSTAG420",
    "WSTAG425",
    "WSTAG430",
    "WSTAG435",
    "WSTAG440",
    "WSTAG445",
    "WSTAG450",
    "WSTAG455",
    "WSTAG475",
    "WSTAG465",
    "WSTAG470",
    # P3.8 batch 4 — South/Suzaku/Network + West/Byakko/North (skip WSTAG460)
    "WSTAG480",
    "WSTAG485",
    "WSTAG490",
    "WSTAG495",
    "WSTAG500",
    "WSTAG505",
    "WSTAG520",
    "WSTAG525",
    "WSTAG530",
    "WSTAG535",
    "WSTAG537",
    "WSTAG540",
    "WSTAG545",
    "WSTAG550",
    "WSTAG555",
    "WSTAG560",
    "WSTAG565",
    "WSTAG570",
    "WSTAG575",
    "WSTAG580",
    "WSTAG585",
    "WSTAG590",
    "WSTAG595",
    "WSTAG600",
    "WSTAG605",
    "WSTAG610",
    "WSTAG615",
    "WSTAG620",
    "WSTAG625",
    "WSTAG630",
    "WSTAG635",
    "WSTAG640",
    "WSTAG645",
    "WSTAG650",
    "WSTAG655",
    "WSTAG660",
    "WSTAG675",
    "WSTAG680",
    "WSTAG685",
    "WSTAG690",
    "WSTAG695",
    "WSTAG700",
    "WSTAG705",
    "WSTAG710",
    "WSTAG715",
    "WSTAG720",
    "WSTAG725",
    "WSTAG730",
    "WSTAG735",
    "WSTAG740",
    "WSTAG745",
    "WSTAG750",
    "WSTAG755",
    "WSTAG760",
}
LIVE_DEFAULT = (
    "0x2d7",
    "0x2d8",
    "0x2d9",
    "0x200",
    "0x270",
    "0x272",
    "0x203",
    "0x204",
    "0x206",
    "0x207",
    "0x208",
    "0x20a",
    "0x20c",
    "0x20d",
    "0x20e",
    "0x20f",
    # batch2
    "0x201",
    "0x271",
    "0x202",
    "0x205",
    "0x274",
    "0x209",
    "0x20b",
    "0x211",
    "0x212",
    "0x213",
    "0x214",
    "0x215",
    "0x216",
    "0x217",
    "0x218",
    "0x219",
    "0x21a",
    "0x21b",
    "0x21c",
    "0x273",
    "0x275",
    "0x276",
    "0x277",
    "0x278",
    "0x279",
    "0x27a",
    "0x27b",
    "0x27c",
    "0x27d",
    "0x27e",
    "0x27f",
    # batch3
    "0x280",
    "0x281",
    "0x282",
    "0x283",
    "0x284",
    "0x285",
    "0x286",
    "0x287",
    "0x288",
    "0x289",
    "0x28a",
    "0x28b",
    "0x21d",
    "0x21e",
    "0x21f",
    "0x220",
    "0x221",
    "0x222",
    "0x223",
    "0x224",
    "0x225",
    "0x226",
    "0x227",
    "0x228",
    "0x229",
    "0x22a",
    "0x22b",
    "0x22c",
    "0x22d",
    "0x22e",
    "0x22f",
    "0x230",
    "0x231",
    "0x232",
    "0x233",
    "0x234",
    "0x235",
    "0x239",
    "0x237",
    "0x238",
    # batch4
    "0x23a",
    "0x23b",
    "0x23c",
    "0x23d",
    "0x23e",
    "0x23f",
    "0x240",
    "0x241",
    "0x242",
    "0x243",
    "0x244",
    "0x245",
    "0x246",
    "0x247",
    "0x248",
    "0x249",
    "0x24a",
    "0x24b",
    "0x24c",
    "0x24d",
    "0x24e",
    "0x24f",
    "0x250",
    "0x251",
    "0x252",
    "0x253",
    "0x254",
    "0x255",
    "0x256",
    "0x257",
    "0x258",
    "0x259",
    "0x25a",
    "0x25b",
    "0x25c",
    "0x25d",
    "0x25e",
    "0x25f",
    "0x260",
    "0x261",
    "0x262",
    "0x263",
    "0x264",
    "0x265",
    "0x266",
    "0x267",
    "0x268",
    "0x269",
    "0x26a",
    "0x26b",
    "0x26c",
    "0x26d",
    "0x26e",
    "0x26f",
)


def parse_us_stages() -> list[tuple[int, int, str]]:
    text = FIELDSTG.read_text()
    start = text.find("StageEntry FIELDSTG_stages[]")
    if start < 0:
        raise SystemExit("FIELDSTG_stages not found")
    body = re.search(r"= \{([\s\S]*?)\n\};", text[start:]).group(1)
    rows: list[tuple[int, int, str]] = []
    for m, f, init in re.findall(r"\{(\d+),\s*(\d+),\s*([^}]+)\}", body):
        mode = int(m)
        if mode == 0:
            continue
        init = init.strip()
        if "StoryEvents" in init:
            name = "WSTAG260"
        else:
            mm = re.search(r"(WSTAG\d+)_", init)
            name = mm.group(1) if mm else init
        rows.append((mode, int(f), name))
    return rows


def classify(name: str) -> str:
    return "linked" if name in LINKED else "missing"


def cmd_inventory(_: argparse.Namespace) -> int:
    rows = parse_us_stages()
    linked = [r for r in rows if classify(r[2]) == "linked"]
    missing = [r for r in rows if classify(r[2]) == "missing"]
    print(f"US FIELDSTG_stages: {len(rows)}")
    print(f"  linked host objs: {len(linked)}")
    for mode, file_id, name in linked:
        print(f"    mode=0x{mode:x} file={file_id} {name}")
    print(f"  missing (weak stubs): {len(missing)}")
    return 0


def run_one(bin_path: Path, mode_hex: str, timeout_s: float, runtime: Path) -> dict:
    env = os.environ.copy()
    env.update(
        {
            "DW3_RUNTIME": str(runtime),
            "DW3_START_MODE": "0x1500",
            "DW3_FORCE_STAGE": mode_hex,
            "DW3_FORCE_STAGE_FRAMES": "60",
            "DW3_AUTO_STAGE_RETURN": "180",
            "DW3_STAGE_SWEEP_EXIT": "1",
            "DW3_PROGRESS": "4",
            "SDL_AUDIODRIVER": "dummy",
        }
    )
    # Prefer a real video driver when available; dummy has no SDL_GPU backend.
    env.setdefault("SDL_VIDEODRIVER", os.environ.get("SDL_VIDEODRIVER", ""))
    if env["SDL_VIDEODRIVER"] == "":
        env.pop("SDL_VIDEODRIVER", None)

    log_dir = ROOT / "build" / "p38-sweep"
    log_dir.mkdir(parents=True, exist_ok=True)
    safe = mode_hex.replace("0x", "")
    stderr_path = log_dir / f"mode_{safe}.stderr"
    t0 = time.time()
    try:
        proc = subprocess.run(
            [str(bin_path), "--runtime", str(runtime)],
            env=env,
            cwd=str(ROOT),
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        rc = proc.returncode
        err = (proc.stderr or "") + (proc.stdout or "")
    except subprocess.TimeoutExpired as e:
        rc = -9
        err = ""
        if e.stderr:
            err = e.stderr.decode("utf-8", "replace") if isinstance(e.stderr, bytes) else e.stderr
        err += "\n[timeout]\n"
    # Keep filtered evidence (GPU spam is huge under dummy).
    filtered = "\n".join(
        ln for ln in err.splitlines() if ("dw3:" in ln or "STAGE_" in ln or "Segmentation" in ln)
    )
    stderr_path.write_text(filtered + "\n")
    elapsed = time.time() - t0
    passed = "STAGE_SWEEP_PASS" in err and "returned to STAGSLCT" in err
    return {
        "mode": mode_hex,
        "rc": rc,
        "elapsed": elapsed,
        "pass": passed,
        "log": str(stderr_path),
        "stagslct_ok": "STAGSLCT" in err and "UNKNOWN" not in err.split("0x176")[0][-80:]
        if "0x176" in err
        else "mode=0x1500" in err,
    }


def cmd_run(args: argparse.Namespace) -> int:
    bin_path = Path(args.bin)
    runtime = Path(args.runtime)
    if not bin_path.is_file():
        print(f"missing binary: {bin_path}", file=sys.stderr)
        return 2
    modes = [m.strip() for m in args.only.split(",") if m.strip()]
    results = []
    for mode in modes:
        print(f"== sweep {mode} ==")
        r = run_one(bin_path, mode, args.timeout, runtime)
        results.append(r)
        status = "PASS" if r["pass"] else "FAIL"
        print(f"  {status} rc={r['rc']} log={r['log']}")
    n_pass = sum(1 for r in results if r["pass"])
    print(f"live: {n_pass}/{len(results)} PASS")
    return 0 if n_pass == len(results) else 1


def cmd_write_results(args: argparse.Namespace) -> int:
    bin_path = Path(args.bin)
    runtime = Path(args.runtime)
    results = []
    for mode in LIVE_DEFAULT:
        print(f"== sweep {mode} ==")
        r = run_one(bin_path, mode, args.timeout, runtime)
        results.append(r)
        print(f"  {'PASS' if r['pass'] else 'FAIL'} rc={r['rc']} log={r['log']}")
    out = ROOT / "build" / "p38-sweep" / "results.tsv"
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# mode\tresult\n"]
    for r in results:
        lines.append(f"{r['mode']}\t{'PASS' if r['pass'] else 'FAIL'}\n")
    out.write_text("".join(lines))
    print(f"wrote {out}")
    return 0 if all(r["pass"] for r in results) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_inv = sub.add_parser("inventory", help="list linked vs missing stages")
    p_inv.set_defaults(func=cmd_inventory)

    p_run = sub.add_parser("run", help="live STAGSLCT round-trip for modes")
    p_run.add_argument("--bin", default=str(ROOT / "build/pc/dw3-pc"))
    p_run.add_argument("--runtime", default=str(ROOT / "runtime/us"))
    p_run.add_argument("--only", default=",".join(LIVE_DEFAULT))
    p_run.add_argument("--timeout", type=float, default=180.0)
    p_run.set_defaults(func=cmd_run)

    p_wr = sub.add_parser("write-results", help="run live defaults + results.tsv")
    p_wr.add_argument("--bin", default=str(ROOT / "build/pc/dw3-pc"))
    p_wr.add_argument("--runtime", default=str(ROOT / "runtime/us"))
    p_wr.add_argument("--timeout", type=float, default=180.0)
    p_wr.set_defaults(func=cmd_write_results)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
