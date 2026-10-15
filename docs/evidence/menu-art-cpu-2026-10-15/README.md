# CPU menu artwork baseline — 2026-10-15

Preserved measurements from the menu composition investigation. This is an
isolated Linux x86_64 CPU experiment, not a Switch/OLED FPS result.
No assets, screenshots, extracted source snapshots or executables are stored.

Source comparison:

- Baseline main: `4b2068fc451e86952e1495c62b96a96427f651f1`.
- PR #170: `b43be1be87d8a6612abaceb5d31c0112784df3c6`.
- Their art generation loops are equivalent; changes concern generation
  tracking and updating one canvas instead of rebuilding five.

GCC 13.3.0, Intel x86_64 family 6/model 141, 8 physical / 16 logical CPUs;
default dynamic frequency and scheduling. Full host details are in
[`environment.txt`](environment.txt). Builds used `-O2` or `-O3`,
`-D__SWITCH__`, `-lm`; the Switch define selects lettering, not target CPU.

## Results

The following **-O3** values are milliseconds, median / p95 / maximum:

- Main five-state batch: cold **49.204 / 52.757 / 57.881**;
  warm **45.090 / 47.276 / 51.527**.
- Main options only: cold **26.981 / 27.578 / 27.819**;
  warm **25.866 / 27.226 / 36.040**.
- PR #170 options only: cold **26.771 / 27.439 / 27.940**;
  warm **26.004 / 27.480 / 31.250**.

Updating one options canvas reduces this isolated CPU median by about 42.3%
against the five-state batch. It is not a fivefold reduction because the
options page dominates drawing. This does not establish Switch frame times.

All six O2/O3 summaries are in [`results.json`](results.json); individual
samples and final checksums are in the six `*-raw.json` files. Metadata,
source hashes and the original experiment's compile arguments are in
[`metadata.json`](metadata.json). These are the original measurements;
the subsequent reusable runner's smoke test did not replace them.

Cold is the first draw in each of 25 independent processes, including buffer
page faults but excluding process startup. It does not mean caches were
flushed. Warm is 200 draws after discarding the first draw in one process.
p95 uses sorted index `floor(.95 * (n - 1))`. Cases ran sequentially, with
no affinity/frequency controls, so load and frequency can affect tails.

The timer includes host pre-clear and the generator's own clear, but excludes
malloc, logging, final checksum, GPU upload/drawing/synchronization/present,
SDL and gameplay. Values/cursor change outside the timer. Built-in English
labels and default footer are used; no EU disc-captured masks were installed.

## Pixel work

[`counts.txt`](counts.txt) comes from a separate untimed instrumented source
copy. Default options cursor 0 / all values OFF generates 2,795,830 pixel
calls on a 691,200-pixel canvas:

- 1,652,136 accepted opaque writes (59.09% of calls).
- 1,117,460 accepted translucent writes (39.97%).
- 26,234 rejected writes (0.94%).

These are calls, not distinct pixels. Large overlapping rectangles account
for repeated work; opaque writes still use generic alpha composition with
integer division. An opaque fast path and static/dynamic layer composition
deserve prototypes; neither was implemented or fidelity-tested here.

One RGBA canvas requires 2,764,800 bytes (2.637 MiB); five require 13,824,000
bytes (13.184 MiB). These are CPU allocation sizes and nominal upload payloads,
not measured GPU bandwidth.

## Reproduce

See the reusable [benchmark instructions](../../../tools/menu_art_benchmark/README.md).
From the repository root:

```sh
python3 tools/menu_art_benchmark/run.py \
  --out /tmp/dw3-menu-art-reproduced --count-pixels
```

The runner extracts the four required source files from the exact commits
into a temporary directory; it needs no assets or initialized submodules.
Results must be written outside the checkout. Expect timing variation;
the saved samples are an observation of this host on this date.
