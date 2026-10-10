# P3.7 Opening Movies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Opening STR movies on PC play with XA audio and Start-skip (`DW3_START_MODE=0xE01`), through PSY-Z libpress + CD stream + port file-backed STR bridge.

**Architecture:** Wire PSY-Z soft MDEC (`Psyz_Mdec*`) under `MDEC_*`; replace port no-op `DecDCTvlc2` / `DecDCTvlcBuild` with a real BS→runlevel path (prefer `vendor/psyz/nugget/bsdec`); implement `StCdInterrupt` + allow stream `CdlSetmode` flags; add `port/` adapter that opens `runtime/.../AAA/STR/*.STR` (2048-byte packed sectors) and feeds the CD ring; pump interrupts from the PC host; prove play + XA + skip before ticking ROADMAP.

**Tech Stack:** CMake/Ninja, host GCC, `vendor/psyz` (pin + intentional submodule commits), `port/cdfs` runtime tree, SDL3 audio, US `runtime/us` (owner disc, not in git).

## Global Constraints

- Spec: `docs/superpowers/specs/2026-10-10-phase3-movies-design.md`
- Evidence baseline: `docs/P3.7_MOVIES.md`
- Branch: work on `phase3-pc-bringup` (or a short-lived `phase3-movies` that merges into it). Docs sync to `main` each wave per owner request.
- Done-when is **play + XA + skip together** — never tick P3.7 for decode-only.
- Keep default boot `MODE_TITLE` (patch `0010`) until done-when is real.
- No fake story-skip past `0xE01`/`0xE02` as a substitute for movies.
- Own-disc only; never commit `runtime/`, discs, or `.STR` assets.
- Submodule edits in `vendor/psyz` must be intentional and called out in commits.
- Portuguese chat; English commits. Stage paths explicitly; commit+push after each task (`AGENTS.md`).

## File map

| Path | Role |
|---|---|
| `vendor/psyz/psyz/src/psyz/libpress.c` | Wire `MDEC_*` → `Psyz_Mdec*`; keep `DecDCTvlc` or route via same VLC stack |
| `vendor/psyz/psyz/src/psyz/mdec.c` / `mdec.h` | Soft MDEC (already built) |
| `vendor/psyz/nugget/bsdec/*` | BS frame decoder for STR (reference / link) |
| `port/psyz_missing/psyz_missing.c` | Today no-op `DecDCTvlc2` / `DecDCTvlcBuild` — replace or move impl |
| `port/movies/` (new) or `port/cdfs/` | File-backed STR open + 2048→sector feed API |
| `vendor/psyz/psyz/src/psyz/libcd.c` | `StCdInterrupt`, `CdlSetmode` Size1/Stream/RT, track open via file bridge hook |
| `platforms/pc/src/*` | Call `StCdInterrupt` when `StCdIntrFlag` (sample pattern); ensure audio init |
| `docs/P3.7_MOVIES.md` | Evidence per wave |
| `docs/ROADMAP.md` | Status sync on `main`; tick only at end |

Game path (do not rewrite): `vendor/dw3_decomp/src/menus/title/movie.c` uses `CdRead2(CdlModeStream\|Speed\|RT\|Size1)`, `DecDCTvlcBuild`, `DecDCTvlc2`, `DecDCTin`/`DecDCTout`.

---

### Task 1: Wire soft MDEC under `MDEC_*`

**Files:**
- Modify: `vendor/psyz/psyz/src/psyz/libpress.c`
- Test: `vendor/psyz/psyz/tests/test_mdec.c` (existing)
- Docs: `docs/P3.7_MOVIES.md` (Wave 1 decode note)

**Interfaces:**
- Consumes: `Psyz_MdecReset`, `Psyz_MdecCommand`, `Psyz_MdecRead` from `mdec.h`
- Produces: real `MDEC_reset` / `MDEC_in` / `MDEC_out` / sync / status matching Psy-Q call patterns used by `DecDCTReset` / `DecDCTin` / `DecDCTout` in `decomp/src/libpress/libpress.c`

- [x] **Step 1: Read how `DecDCTin`/`DecDCTout` call MDEC**

Confirm from `vendor/psyz/decomp/src/libpress/libpress.c`:
- `DecDCTReset` → `MDEC_reset(mode)`
- `DecDCTin` → mutates command word then `MDEC_in(buf, *(u_short*)buf)`
- `DecDCTout` → `MDEC_out(buf, size)`

- [x] **Step 2: Implement `MDEC_*` via soft MDEC**

In `libpress.c`, replace `NOT_IMPLEMENTED` bodies with a mapping such as:

```c
#include "mdec.h"

void MDEC_reset(int mode) {
    (void)mode;
    Psyz_MdecReset();
}

void MDEC_in(u_long* buf, int size) {
    if (!buf || size <= 0)
        return;
    /* First word is MDEC command; remainder are payload words.
       Match test_mdec / hardware: Psyz_MdecCommand(cmd, data, words). */
    uint32_t cmd = buf[0];
    const void* data = size > 1 ? (const void*)(buf + 1) : NULL;
    size_t words = size > 1 ? (size_t)(size - 1) : 0;
    /* Quant/scale tables pass cmd in buf[0] with data following —
       if game/libpress passes full buffer including cmd as first word: */
    Psyz_MdecCommand(cmd, buf + 1, words > 0 ? words : 0);
    /* If iq/coef path passes cmd+payload in one buffer where size counts
       all words including cmd, use: Psyz_MdecCommand(buf[0], buf+1, size-1).
       Align with DecDCTPutEnv which does MDEC_in(mdec_iq, 32) where
       mdec_iq[0] is 0x40000001 — so data is buf+1, words=size-1. */
    (void)data;
}

void MDEC_out(u_long* buf, int size) {
    if (!buf || size <= 0)
        return;
    Psyz_MdecRead(buf, (size_t)size);
}

int MDEC_in_sync(void) { return 0; }
int MDEC_out_sync(void) { return 0; }
u_long MDEC_status(void) { return 0; }
```

Adjust the `MDEC_in` word-count convention until `DecDCTPutEnv` + `test_mdec` semantics agree (iq command `0x40000001` with 32 words total → command + 31 data **or** command included in size as libpress does — verify against `mdec_iq` array length 33 entries / `MDEC_in(..., 32)`).

- [x] **Step 3: Build and run PSY-Z `test_mdec`**

```bash
cmake -S vendor/psyz/psyz -B build/psyz-test -G Ninja
ninja -C build/psyz-test
# run the mdec test binary per that project's test target name
ctest --test-dir build/psyz-test -R mdec --output-on-failure
```

Expected: PASS (or document exact target if ctest name differs).

- [x] **Step 4: Commit + push (submodule note)**

```bash
# If vendor/psyz is a submodule with its own git: commit inside submodule first,
# then bump pointer in parent. If psyz is vendored in-tree, single parent commit.
git add vendor/psyz/psyz/src/psyz/libpress.c docs/P3.7_MOVIES.md
git commit -m "$(cat <<'EOF'
psyz: wire soft MDEC under MDEC_* for libpress

Unblock DecDCTin/out path used by title movies (P3.7 wave 1).
EOF
)"
git push
```

- [x] **Step 5: Sync ROADMAP note on main (in progress, not ticked)**

Docs-only: P3.7 status → in progress / MDEC wired; checkbox stays `[ ]`.

---

### Task 2: Real `DecDCTvlc2` + `DecDCTvlcBuild` (BS → runlevel)

**Files:**
- Create or Modify: `port/movies/dct_vlc.c` (preferred) **or** implement inside PSY-Z and remove port stubs
- Modify: `port/psyz_missing/psyz_missing.c` (remove no-op stubs once real symbols exist)
- Modify: `port/CMakeLists.txt` to compile/link bsdec or port VLC
- Reference: `vendor/psyz/nugget/bsdec/bsdec.c`, `bsdec.h`

**Interfaces:**
- Consumes: STR bitstream from `StGetNext` (`u_long* bs`)
- Produces:
  - `void DecDCTvlcBuild(u_short* table);`
  - `int DecDCTvlc2(u_long* bs, u_long* buf, DECDCTTAB table);`
  - Return 0 on success (match Psy-Q / game expectation in `movie.c`)

- [x] **Step 1: Spike bsdec against one frame from MOVIEOPN.STR**

```bash
# Owner runtime (not in git). Extract first BS frame bytes with a tiny
# throwaway tool or Python under tools/ — do not commit the STR.
python3 - <<'PY'
# optional local probe: open runtime/us/AAA/STR/MOVIEOPN.STR, find BS header
# (word0 looks like MDEC decode cmd). Print bsdecRlHalfwords / decode status.
print("manual spike — document result in P3.7_MOVIES.md")
PY
```

Skipped on bring-up host: `runtime/us` symlink broken / STR absent.
Documented in `docs/P3.7_MOVIES.md`.

- [x] **Step 2: Implement `DecDCTvlcBuild`**

If bsdec does not need Sony’s `DECDCTTAB`, build a valid table shape the game allocates (`STDWTITL_vlcTable`) or no-op fill that `DecDCTvlc2` ignores — but **must not** leave the current silent success that feeds garbage to MDEC. Prefer: Build populates whatever `DecDCTvlc2` reads; if using bsdec-only path, Build can memset/mark ready and `DecDCTvlc2` ignores `table`.

- [x] **Step 3: Implement `DecDCTvlc2` wrapping bsdec**

```c
#include "bsdec.h"
/* Map bsdec output run-level halfwords into buf layout DecDCTin expects
   (1-word MDEC command header + RL data). Return 0 on BSDEC_OK / usable. */
int DecDCTvlc2(u_long *bs, u_long *buf, DECDCTTAB table) {
    (void)table;
    if (bs == NULL) { /* Psy-Q restart convention — see libpress.h */ return 0; }
    /* decode into buf; copy mdecCommand into buf[0]; RL after */
    return 0;
}
```

Remove the no-op from `psyz_missing.c` to avoid duplicate symbols.

- [x] **Step 4: Link into `dw3-pc` and compile**

```bash
cmake --build build/pc --target dw3-pc
```

Expected: link OK; no duplicate `DecDCTvlc2`.

- [x] **Step 5: Commit + push; ROADMAP sync (VLC in progress)**

---

### Task 3: File-backed STR stream + `StCdInterrupt`

**Files:**
- Create: `port/movies/str_stream.c`, `port/movies/str_stream.h`
- Modify: `vendor/psyz/psyz/src/psyz/libcd.c` (`StCdInterrupt`, `CdlSetmode` for Size1/Stream/RT, hook `open_track_at_cd_pos`)
- Modify: `port/CMakeLists.txt`, possibly `port/cdfs/cdfs.c` for path resolve by file id / LBA
- Modify: `platforms/pc/src/main.c` (or present loop) to pump `StCdIntrFlag` → `StCdInterrupt()`

**Interfaces:**
- Consumes: runtime root from cdfs; `CdlLOC` / file id from `FILE_TABLE.getPos`
- Produces:
  - `int dw3_str_stream_open_at_loc(const CdlLOC* loc);` (or equivalent)
  - `int dw3_str_stream_read_sector(uint8_t out[2352]);` synthesizing headers if pack is 2048
  - Working `StCdInterrupt` filling `StSetRing` buffer so `StGetNext` returns frames

- [x] **Step 1: Map movie LBA/file id → `AAA/STR/MOVIEOPN.STR`**

Use existing runtime map / cdfs file table. Document path resolution in `str_stream.h`.

- [x] **Step 2: Sector adapter (2048 → ring view)**

Prefer synthesizing Mode2/Form2 2352-byte sectors from 2048-byte payloads if PSY-Z ring expects them; document choice in `P3.7_MOVIES.md`.

- [x] **Step 3: Implement `StCdInterrupt` + allow stream modes**

In `libcd.c`: stop `NOT_IMPLEMENTED` on `StCdInterrupt`; stop rejecting `CdlModeSize1` / `CdlModeStream` / `CdlModeStream2` when file bridge is active; make `open_track_at_cd_pos` succeed via file bridge for movie LBAs.

- [x] **Step 4: Host pump**

Mirror `vendor/psyz/samples/movie/movie.c`:

```c
extern volatile int StCdIntrFlag;
void StCdInterrupt(void);
/* each frame / CD ready path: */
if (StCdIntrFlag) {
    StCdInterrupt();
    StCdIntrFlag = 0;
}
```

- [x] **Step 5: Smoke `DW3_START_MODE=0xE01` past INIT**

```bash
SDL_VIDEODRIVER=dummy DW3_START_MODE=0xE01 DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Expected logs: no `MDEC_* not implemented`; no `no track found for sector …` for MOVIEOPN; `StGetNext` / frame path advances (may still be silent).

- [x] **Step 6: Commit + push; ROADMAP sync (stream in progress)**

---

### Task 4: XA audio path

**Files:**
- Modify: `vendor/psyz/psyz/src/psyz/libcd.c` (RT/XA pull already partially present)
- Modify: `platforms/pc` audio init path so `Psyz_AudioInit` succeeds for validation runs
- Docs: `docs/P3.7_MOVIES.md`

- [x] **Step 1: Confirm `CdlModeRT` + XA filter path runs during movie**

Game already sets stream with RT. File-bridge sectors deliver XA submode
(`0x64`/`ci=0x01`); `xa_read_raw_sector` uses `Psyz_CdSetReadCB`.

- [x] **Step 2: Run with real SDL audio (not dummy-only)**

```bash
DW3_START_MODE=0xE01 DW3_RUNTIME=runtime/us ./build/pc/dw3-pc --runtime runtime/us
```

On this host SDL has no Pulse/ALSA: `Psyz_AudioInit` falls back to
`dummy`; mix proven with `SDL_AUDIODRIVER=disk` → `sdlaudio.raw`
(peak≈20k). Owner listen on a real DAC still required for done-when.

- [x] **Step 3: Document listen evidence in `P3.7_MOVIES.md`**

Documented decode/mix + owner-listen gap (Wave 4); not faked as audible.

- [x] **Step 4: Commit + push**

---

### Task 5: Start-skip + full done-when evidence + ROADMAP tick

**Files:**
- Modify: `docs/P3.7_MOVIES.md`, `docs/ROADMAP.md` (on `main` sync)
- Possibly: pad inject `DW3_AUTO_PAD` if needed to press Start during movie for automation

- [x] **Step 1: Manual/auto skip**

During playback, Start → movie task kills → `MODE_TITLE` (or next mode from `STDWTITL_movies[]`). Also verify natural end if feasible.
**Done** — `DW3_AUTO_PAD=start` → `dw3-movie: Start-skip` + `mode=0xE00` (Task 5).

- [x] **Step 2: Default boot still `MODE_TITLE` without `DW3_START_MODE`**

Patch `0010` unchanged; default lands `0xE00` first.

- [x] **Step 3: Fill evidence table (play / XA / skip) in `P3.7_MOVIES.md`**

Play + skip + XA mix rows filled; Task 5 used `SDL_AUDIODRIVER=dummy` — owner
DAC listen still **open**. Do **not** tick ROADMAP until audible listen passes.

- [ ] **Step 4: Commit on bringup + docs sync commit on `main` + push both; update PR #4**

Tick ROADMAP `- [x] **P3.7 Movies.**` only after owner audible listen.

---

## Self-review (plan vs spec)

| Spec requirement | Task |
|---|---|
| Wire soft MDEC | Task 1 |
| VLC / DecDCTvlc* real | Task 2 (`DecDCTvlc2`/`Build` — what DW3 calls) |
| File-backed STR + StCdInterrupt | Task 3 |
| XA audible | Task 4 |
| Start-skip + tick only when all three | Task 5 |
| Default MODE_TITLE retained | Task 5 step 2 |
| No fake gate skip | Global constraints |
| ROADMAP sync per wave | Tasks 1/2/3/5 |

## Execution notes

- Prefer Subagent-Driven Development: one fresh implementer per task, then review.
- If `bsdec` license/path integration is awkward inside `port/`, keep a thin wrapper under `port/movies/` that `#include`s nugget sources via CMake `target_sources`.
- Parallelism: do not edit `port/CMakeLists.txt` from two agents at once.
