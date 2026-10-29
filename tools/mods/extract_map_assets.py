#!/usr/bin/env python3
"""MOD map upscale (docs/MOD_MAP_UPSCALE.md): extract the map view's art from
the user's own extracted game data, upscale it with a user-installed
realesrgan-ncnn-vulkan, and write a ready-to-copy SD layout.

    tools/mods/extract_map_assets.py --runtime runtime/us [--runtime runtime/eu] \
        --out tmp/assets/mods/map-upscale/2026-10-28 \
        --upscaler /path/to/realesrgan-ncnn-vulkan --models /path/to/models

What it makes (all derived from the disc: LOCAL ONLY, never commit it):

    OUT/src/<region>/base_h<half>.png          bare map, 392 x 352, opaque
    OUT/src/<region>/<kind>_h<half>_<id>.png   reveal sprites (kind towns|areas)
                                               with alpha, size of the frame box
    OUT/up/...                                 intermediate upscaler folders
    OUT/sd/switch/dw3/mods/<pack>/             copy this folder to the SD card
        mod.json
        map/map_h<half>@4x.png                       shared by US and EU
        map/reveal/h<half>/<kind>_<id>@4x.png        shared sprites
        map/map_<region>_h<half>@4x.png              only where the regions differ
        map/reveal/<region>_h<half>/<kind>_<id>@4x.png

With two --runtime folders every extracted piece (the bare map of each half and
each reveal sprite) is compared pixel for pixel between the regions: identical
pieces are upscaled once and written as shared files, pieces that differ get a
per-region file. The run prints `identical: N of M pieces`. With one --runtime
the pack lists that region only and every file is the region's own (as before).

The reveal sprites are the cells STSTATUS_drawMapScreen draws over the dark map
(map sheet entry 1 = towns, entry 2 = visited-area icons). Alpha is VRAM colour
0 (transparent). The model works on RGB only, so the colour of each sprite is
bled into its transparent texels before upscaling (no dark halo) and the alpha
is upscaled separately as a grey image, then both are recombined. The result is
exactly N times the source size, which is what the loader requires.

Needs Pillow and a C compiler (builds tools/mods/map_assets_dump.c against
port/map_view/map_view_core.c).
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from collections import deque
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PACK_ID = "map-upscale-animevideov3"
SCALE = 4
# FILE_STATUS_BG: the sheet file of half 0; +1 its images, +2 / +3 half 1.
FILE_BASE = {"us": 0x78A, "eu": 0x799}
HALVES = (0, 1)
PAD = 4  # pixels of replicated border around a sprite while the model runs


def build_dump(work: Path) -> Path:
    exe = work / "map_assets_dump"
    cmd = [
        "cc", "-std=gnu11", "-O2", "-o", str(exe),
        str(ROOT / "tools/mods/map_assets_dump.c"),
        str(ROOT / "port/map_view/map_view_core.c"),
        str(ROOT / "platforms/pc/src/title_logo_resample.c"),
        f"-I{ROOT / 'port/map_view'}", f"-I{ROOT / 'platforms/pc/src'}", "-lm",
    ]
    subprocess.run(cmd, check=True)
    return exe


def runtime_files(runtime: Path) -> tuple[str, dict]:
    js = next(runtime.glob("files_*.json"), None)
    if js is None:
        sys.exit(f"{runtime}: no files_<region>.json (is this a runtime/<region> folder?)")
    data = json.loads(js.read_text())
    return data["version"], data["files"]


def pam_to_png(pam: Path, png: Path) -> None:
    data = pam.read_bytes()
    head, _, body = data.partition(b"ENDHDR\n")
    fields = dict(line.split(b" ", 1) for line in head.splitlines()[1:] if b" " in line)
    size = (int(fields[b"WIDTH"]), int(fields[b"HEIGHT"]))
    Image.frombytes("RGBA", size, body).save(png)
    pam.unlink()


def bleed(img: Image.Image) -> Image.Image:
    """RGB of an RGBA sprite with the transparent texels filled from the nearest
    opaque one (multi-source flood), so the upscaler sees no black halo."""
    w, h = img.size
    px = img.load()
    out = Image.new("RGB", (w, h))
    op = out.load()
    seen = [[False] * w for _ in range(h)]
    q: deque[tuple[int, int]] = deque()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            op[x, y] = (r, g, b)
            if a:
                seen[y][x] = True
                q.append((x, y))
    if not q:
        return out
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx]:
                seen[ny][nx] = True
                op[nx, ny] = op[x, y]
                q.append((nx, ny))
    return out


def pad_replicate(img: Image.Image, n: int) -> Image.Image:
    w, h = img.size
    out = Image.new(img.mode, (w + 2 * n, h + 2 * n))
    out.paste(img, (n, n))
    out.paste(img.crop((0, 0, w, 1)).resize((w, n)), (n, 0))
    out.paste(img.crop((0, h - 1, w, h)).resize((w, n)), (n, h + n))
    out.paste(out.crop((n, 0, n + 1, h + 2 * n)).resize((n, h + 2 * n)), (0, 0))
    out.paste(out.crop((n + w - 1, 0, n + w, h + 2 * n)).resize((n, h + 2 * n)), (n + w, 0))
    return out


def run_upscaler(args, src: Path, dst: Path) -> None:
    """One call per folder (or file): animevideov3 x4 on the GPU, low priority."""
    dst.mkdir(parents=True, exist_ok=True) if src.is_dir() else dst.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["nice", "-n", "10", str(args.upscaler), "-i", str(src), "-o", str(dst),
           "-n", args.model, "-m", str(args.models), "-s", str(SCALE), "-g", str(args.gpu),
           "-f", "png"]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def piece_key(path: Path):
    """What makes two extracted pieces the same picture: size and RGBA bytes."""
    img = Image.open(path).convert("RGBA")
    return img.size, img.tobytes()


def compare_regions(out: Path, regions: list[str]) -> dict:
    """{half: {"shared": [names], "region": {region: [names]}, "total": N}}.
    With one region nothing is shared (every piece is that region's own)."""
    res = {}
    for half in HALVES:
        names = {}
        for region in regions:
            for f in (out / "src" / region).glob(f"*_h{half}*.png"):
                names.setdefault(f.name, []).append(region)
        shared, own = [], {r: [] for r in regions}
        for name, have in sorted(names.items()):
            if len(regions) == 2 and len(have) == 2 and (
                    piece_key(out / "src" / have[0] / name) == piece_key(out / "src" / have[1] / name)):
                shared.append(name)
            else:
                for r in have:
                    own[r].append(name)
        res[half] = {"shared": shared, "region": own, "total": len(names)}
    return res


def upscale_sprites(args, dst_name: str, half: int, frames: list[Path], work: Path) -> dict[str, int]:
    """Upscale every sprite PNG in `frames` into map/reveal/<dst_name>;
    returns {name: opaque pixel count}."""
    rgb_in, a_in = work / "rgb_in", work / "a_in"
    rgb_out, a_out = work / "rgb_out", work / "a_out"
    for d in (rgb_in, a_in, rgb_out, a_out):
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
    kept = []
    for f in frames:
        img = Image.open(f).convert("RGBA")
        alpha = img.getchannel("A")
        if not alpha.getbbox():
            continue  # a fully transparent frame: nothing to draw
        kept.append(f)
        pad_replicate(bleed(img), PAD).save(rgb_in / f.name)
        pad_replicate(Image.merge("RGB", (alpha, alpha, alpha)), PAD).save(a_in / f.name)
    run_upscaler(args, rgb_in, rgb_out)
    run_upscaler(args, a_in, a_out)
    dst = args.pack_dir / "map" / "reveal" / dst_name
    dst.mkdir(parents=True, exist_ok=True)
    counts: dict[str, int] = {}
    for f in kept:
        w, h = Image.open(f).size
        box = (PAD * SCALE, PAD * SCALE, (PAD + w) * SCALE, (PAD + h) * SCALE)
        rgb = Image.open(rgb_out / f.name).convert("RGB").crop(box)
        alpha = Image.open(a_out / f.name).convert("L").crop(box)
        out = rgb.convert("RGBA")
        out.putalpha(alpha)
        kind, _, fid = f.stem.split("_h")[0], None, f.stem.rsplit("_", 1)[1]
        out.save(dst / f"{kind}_{fid}@{SCALE}x.png")
        counts[f.name] = sum(1 for v in alpha.getdata() if v > 127)
    return counts


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--runtime", action="append", type=Path, required=True,
                    help="extracted runtime/<region> folder (repeat for us and eu)")
    ap.add_argument("--out", type=Path, required=True, help="local output folder (git-ignored tmp/assets/...)")
    ap.add_argument("--upscaler", type=Path, help="realesrgan-ncnn-vulkan executable")
    ap.add_argument("--models", type=Path, help="its models folder")
    ap.add_argument("--model", default="realesr-animevideov3")
    ap.add_argument("--gpu", type=int, default=0)
    ap.add_argument("--pack", default=PACK_ID, help="folder name under switch/dw3/mods/")
    ap.add_argument("--extract-only", action="store_true", help="stop after writing OUT/src")
    args = ap.parse_args()

    out: Path = args.out.resolve()
    work = out / "work"
    work.mkdir(parents=True, exist_ok=True)
    dump = build_dump(work)
    args.pack_dir = out / "sd" / "switch" / "dw3" / "mods" / args.pack
    regions: list[str] = []
    for runtime in args.runtime:
        region, files = runtime_files(runtime)
        if region not in FILE_BASE:
            sys.exit(f"{runtime}: unknown version {region!r}")
        regions.append(region)
        src = out / "src" / region
        src.mkdir(parents=True, exist_ok=True)
        for half in HALVES:
            sheet = runtime / files[str(FILE_BASE[region] + 2 * half)]["path"]
            imgs = runtime / files[str(FILE_BASE[region] + 2 * half + 1)]["path"]
            subprocess.run([str(dump), str(sheet), str(imgs), str(src), str(half)], check=True)
        for pam in sorted(src.glob("*.pam")):
            pam_to_png(pam, pam.with_suffix(".png"))
    if args.extract_only:
        cmp = compare_regions(out, regions)
        for half in HALVES:
            c = cmp[half]
            print(f"h{half}: identical: {len(c['shared'])} of {c['total']} pieces "
                  f"(regions: {', '.join(regions)})")
        print(f"extracted to {out / 'src'}")
        return
    if not args.upscaler or not args.models:
        sys.exit("--upscaler and --models are required unless --extract-only")

    cmp = compare_regions(out, regions)
    for half in HALVES:
        c = cmp[half]
        print(f"h{half}: identical: {len(c['shared'])} of {c['total']} pieces "
              f"(regions: {', '.join(regions)})")
    for half in HALVES:
        c = cmp[half]
        # the bare map: one file when both regions have the same picture
        for region, base_name in ([(None, f"base_h{half}.png")] if f"base_h{half}.png" in c["shared"] else
                                  [(r, f"base_h{half}.png") for r in regions]):
            src_region = region or regions[0]
            base = out / "src" / src_region / base_name
            dst = args.pack_dir / "map" / (f"map_{region}_h{half}@{SCALE}x.png" if region
                                           else f"map_h{half}@{SCALE}x.png")
            dst.parent.mkdir(parents=True, exist_ok=True)
            run_upscaler(args, base, dst)
            w, h = Image.open(dst).size
            print(f"{region or 'shared'} h{half}: base {Image.open(base).size} -> {w}x{h}")
        shared = [out / "src" / regions[0] / n for n in c["shared"] if not n.startswith("base_")]
        if shared:
            counts = upscale_sprites(args, f"h{half}", half, sorted(shared), work / f"shared_h{half}")
            print(f"shared h{half}: {len(counts)} reveal sprites upscaled")
        for region in regions:
            own = [out / "src" / region / n for n in c["region"][region] if not n.startswith("base_")]
            if own:
                counts = upscale_sprites(args, f"{region}_h{half}", half, sorted(own), work / f"{region}_h{half}")
                print(f"{region} h{half}: {len(counts)} region-only reveal sprites upscaled")

    manifest = {
        "format": 1,
        "id": args.pack,
        "name": f"Map upscale ({args.model})",
        "version": "1.0.0",
        "author": "generated locally from the user's own disc",
        "regions": regions,
        "map": {"scales": [SCALE], "filter": "nearest", "reveal": True},
        "generator": args.model,
    }
    (args.pack_dir / "mod.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"SD layout ready: {out / 'sd'} (copy switch/ to the SD root)")


if __name__ == "__main__":
    main()
