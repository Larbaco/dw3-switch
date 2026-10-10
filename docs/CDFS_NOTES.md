# CD / file serving notes (for P2.5)

Lessons for `port/cdfs` from bringing the decomp up on a host, plus
behaviours the OpenWorld2003 PC recomp already hit in production. None of
this ships game data; it only constrains how we serve the owner's unpacked
tree.

## What the engine expects

See [SURVEY.md](SURVEY.md). Disc I/O is by **numeric file id** over
`FILE_SECTORS` / `FILE_TABLE`, not by path. Completion is
`cdReadyCallback`-style. STR movies still need a sector-stream path
(PSY-Z `libcd` / `St*`).

Our map is `files_<ver>.json` (P1.5) → host files under the P1.6 layout.

## Sector fidelity (steal the checklist, not their code)

OpenWorld2003 1.1.4 documented real bugs from a “close enough” CD layer:

1. **EDC/ECC bytes** — Mode-2 sectors carry 280 bytes of error detection /
   correction after the user data. Filling them with zeros made every sector
   differ from the original disc even when the 2048-byte payload was right.
2. **End-of-file marker** on the last sector of a file — missing EOF broke
   consumers that look for it.
3. **Missing sector ≠ silent zeros** — serving zeros while reporting success
   hid holes; count and fail loud instead.

For P2.5 unit tests (owner data, skipped in CI): compare buffers from
`FILE_CACHE.load(id)` to the disc (or to `extract_disc.py` output) for a
sample of ids, including at least one multi-sector file and its last sector.

If the host path is **payload-only** (2048-byte files, no raw 2352), document
that choice and ensure every caller that expected a full sector gets a
defined transformation. Do not mix “raw sector” and “user data” APIs without
an explicit type.

## DigiLab-style loose-file offsets

JulioGawl’s DigiLab notes (OpenWorld2003 `source/juliogawl/DigiLab/`) express
reward table positions against loose `STFGTREP.PRO` with **2048 useful bytes
per sector**. That matches a payload-oriented layout. Keep the same
convention in any owner-facing editor under `tools/`, and never assume ISO
LBA offsets equal file offsets after extraction.

## Load timing

The original game’s CD latency is part of pacing (place-name cards, fades).
Hosts may offer faster loads later (Phase 6 polish). P2.5 should deliver
**correct bytes and completion callbacks first**; speed-ups are opt-in and
must not change game branches that treat “instant” as a different code path
(OpenWorld2003 hit a typing-speed `0` bug that stalled underwater room
loads).

## Related tasks

- P1.5 / P1.6 — map + pack layout
- P2.5 — CD → filesystem
- P7.2 — patched EU tree through the same pipeline
