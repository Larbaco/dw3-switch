#!/usr/bin/env python3
"""CPU-only menu artwork benchmark; no game assets, display or submodules."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SOURCES = ("title_overlay_art.c", "title_overlay_art.h",
           "title_overlay_labels.inc", "title_overlay_lettering.inc")


def execute(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, **kwargs)


def positive(value):
    number = int(value)
    if not 1 <= number <= 999999:
        raise argparse.ArgumentTypeError("expected an integer in 1..999999")
    return number


def summarize(values):
    ordered = sorted(values)
    return {"n": len(values), "median_ms": statistics.median(values),
            "p95_ms": ordered[math.floor(.95 * (len(values) - 1))],
            "max_ms": max(values)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path,
                        help="new output directory outside the repository")
    parser.add_argument("--main-sha", default="4b2068fc451e86952e1495c62b96a96427f651f1")
    parser.add_argument("--pr170-sha", default="b43be1be87d8a6612abaceb5d31c0112784df3c6")
    parser.add_argument("--cold", type=positive, default=25)
    parser.add_argument("--warm", type=positive, default=200)
    parser.add_argument("--cc", default="cc", help="compiler executable (no shell flags)")
    parser.add_argument("--optimizations", nargs="+", choices=("O2", "O3"), default=["O2", "O3"])
    parser.add_argument("--count-pixels", action="store_true")
    args = parser.parse_args()
    out = args.out.expanduser().resolve()
    if out == REPO or REPO in out.parents:
        parser.error("--out must be outside the repository, including symlink targets")
    if out.exists():
        parser.error("--out must not already exist (preserves earlier evidence)")
    revisions = {}
    for name, revision in (("main", args.main_sha), ("pr170", args.pr170_sha)):
        revisions[name] = execute(["git", "-C", str(REPO), "rev-parse", "--verify",
                                   "--end-of-options", revision + "^{commit}"], text=True).stdout.strip()
    out.mkdir(parents=True)
    metadata = {"created_utc": datetime.now(timezone.utc).isoformat(),
                "source_commits": revisions, "cold_samples": args.cold,
                "warm_samples": args.warm, "optimizations": args.optimizations,
                "platform": platform.platform(), "machine": platform.machine(),
                "compiler": execute([args.cc, "--version"], text=True).stdout,
                "flags": ["-D__SWITCH__", "-lm"], "source_sha256": {},
                "method": "README.md; cold first draw in fresh process; warm discards first draw",
                "harness_sha256": hashlib.sha256((HERE / "bench.c").read_bytes()).hexdigest()}
    try:
        metadata["lscpu"] = execute(["lscpu"], text=True).stdout
    except (FileNotFoundError, subprocess.CalledProcessError):
        metadata["lscpu"] = None
    results = []
    with tempfile.TemporaryDirectory(prefix="dw3-menu-art-benchmark-") as directory:
        temp = Path(directory)
        for name, revision in revisions.items():
            source_dir = temp / name
            source_dir.mkdir()
            for source in SOURCES:
                data = execute(["git", "-C", str(REPO), "show",
                                f"{revision}:platforms/pc/src/{source}"]).stdout
                (source_dir / source).write_bytes(data)
                metadata["source_sha256"][name + "/" + source] = hashlib.sha256(data).hexdigest()
        for opt in args.optimizations:
            for name, five in (("main", True), ("main", False), ("pr170", False)):
                source_dir = temp / name
                exe = temp / f"{name}-{opt}"
                execute([args.cc, "-" + opt, "-D__SWITCH__", "-I" + str(source_dir),
                         str(HERE / "bench.c"), str(source_dir / "title_overlay_art.c"),
                         "-lm", "-o", str(exe)])

                def sample(count):
                    run = execute([str(exe), str(int(five)), str(count)], text=True)
                    values = [float(line.split(",")[1]) for line in run.stdout.splitlines()]
                    if len(values) != count or any(not math.isfinite(x) or x < 0 for x in values):
                        raise RuntimeError("invalid benchmark output")
                    return values, run.stderr.strip()

                cold = [sample(1)[0][0] for _ in range(args.cold)]
                warm_all, buffer_info = sample(args.warm + 1)
                warm = warm_all[1:]
                label = f"{name}-{opt}-" + ("five" if five else "options")
                raw = {"cold": cold, "warm": warm, "buffer_info": buffer_info}
                (out / (label + "-raw.json")).write_text(json.dumps(raw, indent=2) + "\n")
                result = {"label": label, "cold": summarize(cold), "warm": summarize(warm),
                          "bytes": 960 * 720 * 4 * (5 if five else 1)}
                results.append(result)
                print(json.dumps(result), flush=True)
        if args.count_pixels:
            source_dir = temp / "main"
            source = (source_dir / "title_overlay_art.c").read_text()
            needle = "static void pixel(int x,int y,int r,int g,int b,int alpha) {"
            if source.count(needle) != 1:
                raise RuntimeError("pixel instrumentation signature changed; review before instrumenting")
            inserted = ("unsigned long long bench_calls,bench_rejected,bench_opaque,bench_alpha;\n" + needle +
                        "\nbench_calls++;\nif(x<0||x>=960||y<0||y>=720||alpha<=0)"
                        "{bench_rejected++;return;}\nif(alpha==255)bench_opaque++;else bench_alpha++;")
            counted = source_dir / "counted.c"
            counted.write_text(source.replace(needle, inserted))
            exe = temp / "count"
            execute([args.cc, "-O2", "-D__SWITCH__", "-I" + str(source_dir),
                     str(HERE / "count.c"), str(counted), "-lm", "-o", str(exe)])
            (out / "counts.txt").write_text(execute([str(exe)], text=True).stdout)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
