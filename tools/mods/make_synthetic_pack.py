#!/usr/bin/env python3
"""A synthetic map-upscale pack for tests (no game art): a coloured gradient
with a grid every 16 source pixels, at the sizes the loader expects. The replay
runner writes one into the run's config folder (`mods/`) for the `*-map-view-mod`
scenarios; docs/MOD_MAP_UPSCALE.md describes the real pack layout.

    tools/mods/make_synthetic_pack.py DEST_MODS_DIR [--scale 4] [--regions us eu]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

SRC_W, SRC_H = 392, 352


def synthetic_map(half: int, scale: int) -> Image.Image:
    w, h = SRC_W * scale, SRC_H * scale
    img = Image.new("RGB", (w, h))
    px = img.load()
    step = 16 * scale
    for y in range(h):
        for x in range(w):
            if x % step == 0 or y % step == 0:
                px[x, y] = (255, 255, 255)
            else:
                px[x, y] = (x * 255 // w if half == 0 else 255 - x * 255 // w,
                            y * 255 // h, 96 + half * 80)
    return img


def make_pack(mods_dir: Path, regions=("us", "eu"), scale: int = 4, name: str = "synthetic",
              shared: bool = False) -> Path:
    """shared=True writes the one shared map per half (map_h<half>@Nx.png, P51
    item 3) instead of one file per region; the manifest lists the regions."""
    pack = mods_dir / name
    (pack / "map").mkdir(parents=True, exist_ok=True)
    for half in (0, 1):
        img = synthetic_map(half, scale)
        if shared:
            img.save(pack / "map" / f"map_h{half}@{scale}x.png")
            continue
        for region in regions:
            img.save(pack / "map" / f"map_{region}_h{half}@{scale}x.png")
    manifest = {"format": 1, "id": "synthetic-test", "name": "Synthetic test pack",
                "version": "0.0.1", "regions": list(regions),
                "map": {"scales": [scale], "filter": "nearest", "reveal": False},
                "generator": "tools/mods/make_synthetic_pack.py"}
    (pack / "mod.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return pack


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dest", type=Path)
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--regions", nargs="+", default=["us", "eu"])
    ap.add_argument("--shared", action="store_true", help="one shared map per half, no per-region files")
    a = ap.parse_args()
    print(make_pack(a.dest, tuple(a.regions), a.scale, shared=a.shared))


if __name__ == "__main__":
    main()
