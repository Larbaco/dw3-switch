# P6.25 Cross-region saves (design, survey and slice 1 as built)

Owner decision (2026-10-24, recorded in [SINGLE_BUILD.md](SINGLE_BUILD.md)):
**saves must be compatible between the US and the EU game**: a save made in one
region loads in the other. This note is the survey of how the two save images
differ, taken field by field from the decomp, and the design that follows. It
is the base of the [ROADMAP P6.25 entry](ROADMAP.md#phase-6-goal-notes).
**Slice 1 is built (2026-10-25, see [As built](#as-built-slice-1)):** the
converter and a one-way import on load. The `.dw3s` container, the shared
store and the write-back are **not** built yet. P6.24 (one build) and
[P6.26](PER_USER_SAVES.md) (per-user saves) depend on the rest.

Sources: `vendor/dw3_decomp` at `b9caa6c`, `origin/main` at `545ede2`, and one
real US save of the owner read on the PC (offsets and checksum below were
checked against it; no save content is copied anywhere). The EU side comes from
the decomp only: **no EU save was opened**, see "Not verified".

## As built (slice 1)

Date 2026-10-25, branch `feat/p6.25-cross-region-saves`. The design below
recommends, as its order step 1, the converter and its tests; step 2 is the
store. Slice 1 is step 1 **plus the smallest loader that uses it**, so a US
save loads in the EU build and the reverse, without the container. No decomp
patch was needed (numbers 0130-0134 stay free).

**What it does.**

- `port/memcard/save_convert.{c,h}`: pure functions on byte buffers.
  `save_convert_image` takes a 32 KiB card image (`saveN.bin`) and produces the
  image in the layout of a target region: header and icon sectors verbatim;
  info section with the target version byte and a new checksum; each data slot
  that has a name in the info section is detected by its version byte and its
  **exact** XOR checksum (3 = US, 4 = EU) and converted by the flag-group table
  (the table in "Different (counts)" above, in code as `g_save_flag_groups`),
  version byte and checksum redone for the target; the runtime tail after
  `GAME_SAVE_SIZE` is cleared. A slot already in the target layout is copied as is.
- Not converted, kept byte for byte so the game shows its own error 8: a slot
  with an unknown version or a wrong checksum, and an EU slot with `progress`
  of 0x2D or more (the extra chapter) going to US. A source that is not a DW3
  card (no `DMW3` magic, wrong info checksum, wrong size) is refused whole.
- `memcard_host_ensure(port)`: when the port has **no `saveN.bin` of its own**
  (after the `.mcr` migration found nothing) and an import folder is set, it
  reads that folder's `saveN.bin`, converts it in memory to this build's
  region, and writes it as this port's `saveN.bin` (rename-atomic, the normal
  flush). Then it writes `saveN.import` (text: format 1, `converter=1`, from,
  to, source, per-slot results). The source file is opened read-only and never
  written, renamed or deleted. If nothing usable results (no usable slot) or
  anything fails, nothing is written and the card stays empty.
- Hosts: `memcard_host_setup_cross_import` (called by `platforms/pc/src/main.c`
  and `platforms/switch/source/boot_main.c`). With the default folders the
  source is the sibling of the runtime root: the EU build reads
  `.../us/memcard/saveN.bin` and the US build reads `.../eu/memcard/saveN.bin`
  (`sdmc:/switch/dw3/<us|eu>/memcard/`, PC `runtime/<us|eu>/memcard/`).
- Test hooks (PC): `DW3_MEMCARD_IMPORT_DIR=<dir>` names the source folder,
  `DW3_CROSS_IMPORT=0` turns the import off.

**Defaults chosen, owner can change.**

| # | Default | Why / how to change |
|---|---|---|
| 1 | No `.dw3s` container, no shared store yet: the converted copy is this region's own `saveN.bin` | Smallest safe slice; the container is design step 2 and is still open |
| 2 | Import only for a port that has **no** save file of its own. An existing `save0.bin` is never changed, merged or replaced, so a player who already has saves in both regions keeps both sets apart | Never risks an existing save. Merging slots into an existing card needs the store (slice 2) and the backups it plans |
| 3 | At most once per port: `saveN.import` is the version marker and the "already imported" flag, so a save the player deletes or formats does not come back. Delete `saveN.import` (and the card) to import again | A deleted save must stay deleted |
| 4 | On by default in the default layout (sibling folders); off for any run that sets `DW3_MEMCARD_DIR`/`--memcard-dir` (replays) unless `DW3_MEMCARD_IMPORT_DIR` is also set; `DW3_CROSS_IMPORT=0` disables it. There is no in-game switch yet | The Switch has no environment, so there it is always on. An Options row is a later item if wanted |
| 5 | **Saving writes this region's native format only, into this region's folder.** A save made after the import does not flow back to the other region (one-way copy; the original stays as it was) | The design's single store needs the container; two-way sync is slice 2. Say it in the release notes |
| 6 | EU slot with `progress >= 0x2D` into US: not converted, bytes kept, the US game shows error 8; a card with only such slots imports nothing | As the design (hard case 1) |
| 7 | Flag bytes with no place in the target (EU extra-chapter flags into US, US flags40 bytes 0x1A-0x1B into EU) are dropped from the copy with a log warning, not refused | Design hard cases 2 and 3: they are unused/extra-chapter flags; the original keeps them |
| 8 | The imported card has no `saveN.name` sidecar, so any DW3 product name matches (it is a "naked" save, as before the sidecar existed) | EU asks for a name by language (`BESLES`/`BASLUS`/`BISLPS`); this also avoids the language-switch limit for imported cards |
| 9 | Checksums of a foreign-format slot and of the info section must be **exact**; the game itself only checks `computed & ~stored` | Fail closed: nothing is "repaired" |
| 10 | The info section checksum covers bytes 4 to 0xD4 (`sizeof(MemCardFile)`), not 0x100 as the survey said above (`saves.c:231-235`, `:507`); checked on a save the game wrote | Corrected here |

**Risks to existing saves, and how they are handled.**

- The import only runs when the port's own `saveN.bin` does not exist, so no
  existing own-region save can be touched. The source is read-only. The write
  is the same rename-atomic flush as any save; a crash leaves no `saveN.bin`
  or a complete one (a leftover `.tmp` is ignored), and the marker is written
  after the save, so an interrupted import runs again.
- A corrupt, truncated, random or wrong-size source is refused before anything
  is written (unit tests: noise, zeros, short, a directory, bad info checksum
  or magic, only-unusable slots).
- The sidecar-less card accepts any product name: if two builds of different
  regions share one folder by hand, they see the same card, as they would for
  a naked `save0.bin` today.
- The converted copy of an EU save in the US build loses the 10 bytes of EU-only
  flags (see 7); the EU original still has them.
- Downgrade: an older build ignores `saveN.import` and keeps the copy it finds.

**Tests.** `tools/tests/test_save_convert.py` (35 tests, synthetic saves
generated in pytest temp directories, no game bytes): every flag bit of every
group moves by `(group, id)` in both directions; common block, version,
checksum and tail; round-trip identity when nothing is dropped; extra chapter
refused with bytes kept; invalid and corrupt inputs; file-level conversion
through `tools/save_convert_tool.c` (never overwrites, never in place); the
layout table against `vendor/dw3_decomp/include/engine/game_state.h` and
`memory_card.h` (skipped when the submodule is absent); the host import
(`tools/tests/cross_import_main.c`: copy made, original hash unchanged,
marker, existing save kept, not back after delete, corrupt sources, both
ports, sibling folders, environment switches). Replay scenarios
([P6.15](P6.15_REPLAY_TESTS.md)): `xr-us-to-eu`, `xr-eu-to-us` (the game saves
on one disc through the forced save screen; the other disc imports it,
Continue loads it and the field of the saved mode is reached, log verdicts),
`xr-eu-extra-us` and `xr-us-badsum-eu` (refusals: empty card, no crash). On a
real US save of the owner, read only: layout, version and checksum confirmed,
US to EU to US converts with no dropped bytes.

**Not done in slice 1** (open): the `.dw3s` container and its per-slot origin
metadata; merging foreign slots into an existing card with backups; the
first-fit import of up to six slots; two-way flow; an Options switch; the
Ryubing runner for the cross scenarios (they need one card folder per region;
PC only today); a real EU save was still not available, so the EU layout stays
from the decomp, with the EU build's own game-written save as the check.

**OLED checklist (owner, US and EU builds from this PR).** Both builds on the
same SD card, with a US save in `sdmc:/switch/dw3/us/memcard/` and no EU save
in `sdmc:/switch/dw3/eu/memcard/`:

1. Start the EU build: `dw3.log` has `memcard cross-import slot=0 from=us to=eu`
   and a `save0.import` file appears in the EU folder; the US `save0.bin` is
   unchanged (compare its size and time stamp).
2. Title, Continue, memory card slot 1: the US save is listed (name, area,
   money, play time); load it: the game continues in the field at the saved
   place. Save over it, quit with HOME, start again, Continue: it loads.
3. Reverse: with an EU save and no US save, start the US build: the same.
4. With a save in both folders: nothing is imported (each build keeps its own).
5. An EU save from the extra chapter (if available): the US build shows no
   save (or error 8 if the card also has an importable one) and does not crash.

## What a save is today

Port side (`port/memcard/`, [README](../port/memcard/README.md),
[P5.3](P5.3_FLAT_SAVE.md)):

- One flat file per memory-card port, `save0.bin` / `save1.bin`, 32 KiB
  (0x8000), resident in RAM; `MemCardReadFile` / `MemCardWriteFile` are
  `memcpy` at an offset (`memcard_libmcrd.c`). Flush is rename-atomic.
- A sidecar `saveN.name` holds the PS1 product name the game asked for.
  `name_matches()` in `memcard_libmcrd.c` compares it with `strcmp`: a read or
  write that uses another name fails with `McErrFileNotExist`.
- Directory: `<root>/memcard`, i.e. `sdmc:/switch/dw3/<us|eu>/memcard/` and
  `runtime/<ver>/memcard/` (`boot_main.c` ~531, PC `main.c` ~282;
  `DW3_MEMCARD_DIR` overrides). One region never sees the other's folder.

Game side (`vendor/dw3_decomp`, identical in both regions unless noted):

- `initMemCard` (`src/engine/memcard/memcard.c`): `infoSize` 0x100, `dataSize`
  0x2700. Image offsets: header sector 0x00 ("SC", title[64] in Shift-JIS, from
  string 0x27 of `TEXT_FILE(TEXT_MEMORY_CARD)`, `screen.c:105-123`), icon
  sectors, info section at **0x200**, data slot n at **0x300 + n * 0x2700**
  (three slots, ending at 0x7800). **Offsets and strides are the same in US and
  EU.**
- Info section (`MemCardFile`, `include/menus/memory_card.h`): checksum u8 (XOR
  of bytes 4..0xD4, see "As built" item 10), last-slot u8, version u8, magic `DMW3` (0x33574D44), then
  three `MemCardSave` summaries (name[0x18], area, place, money, play time,
  partners, levels). Same layout in both regions. Loading checks the magic and
  the checksum only (`saves.c:225-235`).
- Data slot = `GAME` bytes `0 .. GAME_SAVE_SIZE` (`GameSave`). `GameState.checksum`
  (byte 0) is the **XOR of bytes 4 .. GAME_SAVE_SIZE** (`computeChecksum`,
  `memcard.c:444`); `GameState.version` (byte 2) is `MEMCARD_SAVE_VERSION`:
  **3 in US, 4 in EU** (`memory_card.h:97-101`). `STGMCARD_loadSave`
  (`saves.c:389-405`) rejects a slot with another version, or a wrong checksum,
  with **error 8**. The checksum length is `sizeof(GameSave)`, which is
  region-dependent. So a raw cross-region load fails on the version and on the
  checksum, and even a patched version byte would load a shifted flag area.
- `writeSave` writes the full 0x2700 of `GAME`, so the **tail after
  `GAME_SAVE_SIZE` is runtime state** (`mode`, `nextMode`, ...), never read back.
  The real US slot checked had non-zero bytes there. A converter must treat the
  tail as don't-care.
- PS1 product name by region: US `BASLUS-01436DMW3-USA` always; EU picks by
  `LANGUAGE` (`memcard.c:34+`): 0 `BISLPS-03446DMW3-JPN`, 1 `BASLUS-01436DMW3-USA`,
  2 to 6 `BESLES-03936DMW3-EUR`.

## The save-difference list

### Identical (no work)

| Area | Evidence |
|---|---|
| Card image offsets, info section, slot stride | `initMemCard`, `readSave` / `writeSave`; same code |
| `GameState` bytes **0x0000 to 0x2643** (9,796 bytes): checksum, version, `fieldMode`, `fieldPos`, `place`, `playTime` (0x48), player `name` (0x54), `money` (0x6C), `party`, `items[0x193]` (0x7C), `equippedItems` (0x20F), `cards[0x13D]` (0x3A2), `cardsSeen` (0x4DF), `decks[3]` (0x628), `partners[8]` (0x75C, 0x3DC each), `progress` (0x263C), `partySet` (0x2640) | `include/engine/game_state.h`: the only region blocks in the header are the flag groups, `GAME_SAVE_SIZE` and the `MODE_*` ids (lines 435, 504, 566); `Partner`, `Deck`, `PlayTime` have none |
| Item, tech, Digimon and card id spaces | `ITEMS`, `TECHS`, `DIGIMON_DATA` have no region block; sizes equal (`symbols.txt`: ITEMS 0x12D8, TECHS 0x1F26); `cards` / `cardsSeen` arrays have the same size; the card-game blocks in `src/cardgame` are layout, file ids and rules, not card ids (`cardgame_3.c`) |
| Stage mode ids 512..623 and the 239 normal field modes | `FIELDSTG_stages` has the **same 239 modes and stage names** in both regions (`fieldstg.c:2799` US, `:3259` EU), so `fieldMode`, `place` and the visited flags carry over |
| Play time | `PlayTime.frames` is 8.8 fixed seconds*60; EU in PAL adds 0x133 per vsync, NTSC 0x100 (`display.c:14-28`), so it is already time, not frames |
| Info-section `area` / `place` | indices into `TEXT_AREA_NAMES` / `TEXT_SHOP_NAMES`, `STGMCARD_places` and `STGMCARD_prevModes` have no region block (text order: see "Not verified") |
| Battle state | `BattleFuncs` differs (EU `rollCounter` at 0xD0, `battle.h:352`) but it is battle runtime, **not in `GameState`**; the same for the `title.h` and `field_menu.h` region members and for the EU-only `randomGauges` (0x26F8) |
| Language | `LANGUAGE` (2 to 6) is **not saved**; the EU build chooses it per session at `CNTY_SEL` (P6.2) |

### Different (counts)

1. **Flag area, offset 0x2644 on.** 13 bitset groups packed back to back
   (`checkCondition` / `applyAction`, `events.c:500-640`; group =
   `code >> 8 & 0xFE`, bit = `code & 0x1FF`). 0x78 bytes in US, 0x80 in EU
   (`GAME_SAVE_SIZE` **0x26BC US, 0x26C4 EU**):

   | group | US off / size | EU off / size | change |
   |---|---|---|---|
   | 0x02 | 0x2644 / 0xD | 0x2644 / 0x12 | +5 |
   | 0x04 | 0x2651 / 2 | 0x2656 / 2 | moved +5 |
   | 0x06 | 0x2653 / 1 | 0x2658 / 1 | moved +5 |
   | 0x08 | 0x2654 / 1 | 0x2659 / 1 | moved +5 |
   | 0x0A | 0x2655 / 2 | 0x265A / 4 | +2 |
   | 0x0C | 0x2657 / 8 | 0x265E / 8 | moved +7 |
   | 0x0E | 0x265F / 0xC | 0x2666 / 0xC | moved +7 |
   | 0x10 | 0x266B / 2 | 0x2672 / 4 | +2 |
   | 0x18 | 0x266D / 1 | 0x2676 / 2 | +1 |
   | 0x1A | 0x266E / 9 | 0x2678 / 9 | moved +10 |
   | 0x1C | 0x2677 / 0xB | 0x2681 / 0xB | moved +10 |
   | 0x20 | 0x2682 / 0x1E | 0x268C / 0x1E | moved +10 |
   | 0x40 | 0x26A0 / 0x1C | 0x26AA / 0x1A | **-2** |

   4 groups grow (+10 bytes), 1 shrinks (-2), 8 keep their size and only move.
   Net +8.
2. **Version byte 3 / 4** and **the checksum** (different length, so a
   different value) of every slot. Both are recomputed by the converter.
3. **Slot tail** after `GAME_SAVE_SIZE`: runtime, ignored.
4. **PS1 file name and header title**: language- and region-dependent, only
   used by the port as the `name_matches` key (see "Name binding").
5. **Glyph images of names** (below). The bytes are the same encoding.
6. **EU-only content in the same fields**: the extra chapter.

### Flag ids: what each region uses (checked)

A script over every `.c` in `src` honoring `#if VERSION_US` / `#elif VERSION_EU`
and limited to the stages each region builds (`config/<ver>/stages.txt`; the 55
extra stage files exist in the tree but are built only for EU) found:

- **544 distinct `FLAG(group, id)` references in US, 596 in EU** (556 / 608 with
  group 0, the non-saved temporary `FLAGS_00`). **Every US flag is also an EU
  flag; EU has 52 more.** Those 52 are the extra chapter: group 0x02 ids
  0x66-0x7C (23), 0x0A ids 0xE-0x1B (14), 0x10 ids 0xC-0x16 (11), 0x18 ids 5-7
  (3) and 0x40 id 0xCD (1). They are exactly the bits that fall in the groups
  that grow in EU.
- The `Conditions` / `Actions` lists of the stages use the `FLAG()` macro for
  every flag group; the raw 4-hex-digit literals in them are other groups
  (shop `0x7A..`, items `0x8E..`, events) and no raw literal in the flag groups
  was found (a grep, not a parse: a flag written as a bare number would escape
  it). The visited flags use `FIELD_VISITED_FLAG(mode) = 0x2000 + mode - 0x200`
  (`field_mode.h:659`, group 0x20, `field.c:515`), ids up to 0xEE of 240 bits,
  in range in both.
- Every US id fits the **EU** group sizes. The highest id used in group 0x40 is
  0xCC in US and 0xCD in EU, both under the 0xD0 bits that EU keeps. **No script
  references flags40 bits 0xD0 to 0xDF**, so the two bytes EU drops (flags40
  bytes 0x1A and 0x1B of the US layout) are unused. They may still hold stray
  bits; the converter checks them.
- Same id, same meaning: of the 572 source files both regions compile (stages
  plus engine), **571 use the identical multiset of flag references**. The only
  one that differs is `WSTAG395` (`wstag395.c:605-609`): the EU condition list of
  `actor0` also requires `FLAG(0x40, 0x11)` to be 0 (a gameplay fix; no layout
  effect).

### Hard cases

1. **EU extra chapter (`progress == 0x2D`)**. Set by `endEvent1500` in
   `wstag780.c:622-627`, **EU only**. In US there is no stage table for it
   (`FIELDSTG_extraStages`, 55 stages WSTAG920-974, modes 624-750,
   `fieldstg.c:3199`; picked by `FIELDSTG_pickStage`, `state.c:23-31`) and the
   title movie jumps to mode 0x276 (`movie.c:241`). A US build **cannot play**
   such a save. Proposal: the US view of that slot is "other save version"
   (error 8, the game's own text) and the bytes stay untouched in the store.
   US to EU has no equivalent problem: a US save is the base game, and EU
   unlocks the extra chapter itself at event 1500.
2. **US flags40 bytes 0x1A-0x1B** (US to EU drops 2 bytes). Unused by any script
   (above); the converter checks that both are zero and, if not, keeps them in
   the per-slot metadata so US to EU to US is lossless.
3. **EU to US drops 10 bytes of flag groups** (02: 5, 0A: 2, 10: 2, 18: 1).
   Everything in them is an extra-chapter flag; they are zero in any save that
   never entered the extra chapter. A save with progress < 0x2D and one of them
   set would be a bug of the converter's assumption: log it, keep it in the
   per-slot metadata.
4. **Names and glyphs.** Names are Shift-JIS (`text_tools.c`), the same bytes in
   both regions, and `FONT_GLYPH_MAP` is shared, but **26 of the 230**
   `STPLNMET_nameGlyphs` entries differ (`stplnmet.c`; e.g. indices 165-168,
   188-192, 209-213: katakana slots the EU font reuses for accented letters). An
   EU name with accents shows other glyphs in US. Affected: the player name
   (0x54), partner names (`PartnerStats.name[0x18]`), deck names
   (`Deck.name[0x16]`), `MemCardSave.name`. Maximum length is 8 (5 in the EU
   Japanese language, `name_entry.c:55-64`). Proposal: no conversion (cosmetic);
   say so in the notes. `STDGNAME_nameGlyphs` has 108 region lines, but they
   are glyph atlas coordinates, not default names.
5. **EU Japanese language (`LANGUAGE` 0)**: kana names; whether the US font covers
   them was not checked.
6. **Name binding in the port.** `name_matches` means that an EU save made with
   language 2 to 6 (`BESLES-...`) is invisible, and not writable
   (`McErrFileNotExist`), when the next session picks language 0 or 1 (other
   product name). Static reading of the code, **not reproduced**; if true it is
   an existing EU limitation that the new store removes (see "Name binding").
7. **Checksum quirk.** The game checks `computeChecksum(...) & ~stored` (not
   `!=`): a wrong checksum passes if the computed bits are a subset of the
   stored ones. The converter must write the exact XOR, not rely on that.

## Design

### Canonical format

A port-side container, **not** a PS1 card image, with the **EU layout as the
canonical data** (it is the superset: US data fits it without loss, except two
unused bytes handled by the metadata):

```
saves/<profile>/card0.dw3s, card1.dw3s       (profile: P6.26; "shared" until then)
  +0x00  "DW3S"                  magic
  +0x04  u16 format version      1
  +0x06  u16 header size
  +0x08  u32 payload size        0x8000
  +0x0C  u32 crc32 of the payload
  +0x10  u8  last writer region  1 US, 2 EU
  +0x11  u8  last writer language (EU LANGUAGE 0..6, 0xFF none)
  +0x12  u16 reserved
  +0x14  per slot (3): { u8 origin region, u8 origin language,
                         u8 flags (bit0 needs EU extra chapter,
                                   bit1 has US flags40 tail),
                         u8 us_flags40_tail[2] }
  +0x40  payload: the 32 KiB card image, header / icon / info sections as the
         game writes them, data slots in the EU layout (version 4, EU checksum)
```

Unknown trailing header bytes are ignored and a larger format version is read
read-only (never rewritten), the same forward rule as `port_options.cfg`.

Why not US canonical: it loses the extra chapter. Why not a TLV per group:
more code, and it buys nothing because the ids and meanings are shared (above).
Why a container and not just the EU image: the metadata (origin, the two US
bytes, "needs extra chapter") has to live somewhere that the game's full-slot
write (it writes 0x2700 bytes and rewrites the whole slot) cannot overwrite.

### Where the conversion happens

At the **host boundary, once per open and once per flush**, not per sector. The
game keeps talking to `MemCard*` and sees a card in **its own region's native
format**:

- `memcard_host_ensure(slot)` reads the `.dw3s`, checks the crc, and builds the
  native RAM image: sections 0x00-0x2FF verbatim; for each of the 3 slots
  `canonical_to_native(region)`.
- `memcard_host_flush` converts back only the slots the game **wrote** in this
  session (tracked per slot by the sector writes in `memcard_host_file_write`),
  and copies the rest unchanged. A slot the running region cannot load (the
  extra chapter in US) is presented with version byte 0xFF, so the game shows its
  own error 8, and it is never rewritten unless the player saves over it.
- The info section is rebuilt from the slots it describes only when a slot
  changed (checksum = XOR of bytes 4..0xD4).

`canonical_to_native(US)`: copy bytes [0, 0x2644) except byte 2 (=3); copy each
flag group `min(size)` bytes from its EU offset to its US offset, zero-extend;
refuse if `progress` (0x263C) is 0x2D; restore the US flags40 tail from the
metadata; set the checksum (XOR of bytes 4..0x26BC). `native_to_canonical(US)`
is the reverse (zero-extend each group, byte 2 = 4, tail check, checksum over
0x26C4 bytes). EU is the identity on the data, plus the checks. The function is
pure C on byte buffers (`port/memcard/save_convert.c`), with a table of
`{group, us_off, us_size, eu_off, eu_size}` copied from this note and asserted
against the decomp headers by a unit test (`GAME_SAVE_SIZE`, offsets).

### Name binding

The new store drops the strict name check: any of the DW3 product names
(`BASLUS-01436DMW3-USA`, `BESLES-03936DMW3-EUR`, `BISLPS-03446DMW3-JPN`, and the
`BESLPS-99999...` / `BISLPS-99999DMW3-JPN` names of the US build's table) is the
same file, and the name the running build asks for is just echoed back. This
also removes the EU language-switch limitation (hard case 6). The header title
stays as the last writer made it (cosmetic, the port never shows it).

### Migrating existing saves

Existing per-region saves must keep working, and the originals are never
changed:

1. Sources: `<root>/us/memcard/saveN.bin` and `<root>/eu/memcard/saveN.bin`
   (N = 0, 1). Nothing is moved, renamed, or deleted: the legacy folders stay
   as they are (the `.mcr.bak` of the P5.3 migration included).
2. First launch of a build with the store, if the profile has no `card0.dw3s`
   and no `imported.txt`: read the sources (US A, EU A, US B, EU B, in that
   order), take every **non-empty valid slot** (version byte and checksum OK for
   its source region) and **first-fit** them into the 6 target slots (card A
   slots 0-2, then card B slots 0-2). Convert each to canonical and fill the
   metadata (origin region; language 0xFF, since it is not in the save).
   Rebuild both info sections.
3. More than 6 non-empty slots in total is rare but possible: the extras are not
   imported, stay in the legacy folders, and are listed in `imported.txt` and the
   log. A `tools/import_saves.py` (PC, later) can place them by hand.
4. Write `imported.txt` (what came from where, when) last, so an interrupted
   import runs again. A backup of any `.dw3s` is taken before a slot of foreign,
   non-lossless origin is overwritten (rotating `card0.dw3s.bak1..3`).
5. Downgrade: an old build keeps reading the legacy folders, which now stop
   getting new saves. Say it in the release notes; no dual write (open question).

With P6.26 the import target is the first user who launches (prompted); see
[PER_USER_SAVES.md](PER_USER_SAVES.md).

### EU language setting

Not in the save, not converted. Each session picks it at `CNTY_SEL`. The origin
language is kept in the per-slot metadata for information, and for a future
"warn if the language changed" message, nothing more.

## Is it useful before the single build?

**Yes, and it should come first.** It needs no single image: both separate
builds (US and EU) can use the same store folder `sdmc:/switch/dw3/saves/shared/`
(PC `runtime/saves/shared/`), each converting to its own layout. Gains before
P6.24: a player with both discs can continue one game on either; the EU
language-switch limitation goes away; the EU extra chapter can be tested from a
US save; the converter and its tests mature in the existing preview cycle
instead of arriving together with the build rewrite. The cost of landing it
early is only the downgrade note above.

## Risks

- **Data loss.** Mitigated: originals untouched; the legacy folders are the
  fallback; backups before overwriting foreign slots; atomic flush; crc on the
  container.
- **Wrong layout assumption.** The layout table is checked against the decomp
  headers by a unit test, and the real-save check (below) catches a mismatch.
- **Semantic drift** (same id, other meaning): checked for flags (571 of 572
  files identical), not for every state field; the real-save round trip on both
  regions is the guard.
- **Replay tests (P6.15).** They set `DW3_MEMCARD_DIR` and must keep doing so:
  an explicit directory means "this folder is the store, no import, no
  profile, no profile selector, no region picker". Round trip native -> canonical
  -> native must be identical over `[0, GAME_SAVE_SIZE)` and the sections, so the
  references stay valid.
- **Interchange tool P9.6** (`.mcr` import/export) should target the canonical
  container, not the flat file, once this lands.

## Test plan

Synthetic only; no save or game data enters git.

1. **Unit tests** (`tools/tests`, or a C test linked on the PC): a generator that
   fills a `GameState` image per region from the decomp offset table with
   distinct byte patterns per field, sets version and checksum, and then:
   US -> canonical -> US and EU -> canonical -> EU are the identity over
   `[0, GAME_SAVE_SIZE)`; US -> EU moves every flag bit by `(group, id)` (all 13
   groups, every bit set once); EU -> US drops only the documented bytes;
   `progress == 0x2D` is refused in US with the bytes preserved; the flags40
   tail is checked and preserved; a bad checksum, a bad version and a short
   file are reported, not read past; checksum and version are exactly what the
   target region's `STGMCARD_loadSave` accepts (re-implemented XOR, plus the
   offsets asserted against `include/engine/game_state.h`).
2. **Host tests** for the store: crash between `.tmp` and rename, a truncated
   `.dw3s`, the crc, the first-fit import (0, 1, 6 and 7 non-empty source
   slots), re-run after an interrupted import, backups before a foreign
   overwrite, an unknown newer format version.
3. **Game round trip on the PC (local, needs the owner's discs).** A replay
   scenario that makes a save from New Game on the US build, a tool converts
   it, and an EU scenario does Continue and reaches the field; and the reverse.
   The save is produced by the test run in a temp folder, so nothing is
   committed.
4. **Real saves, local only.** A script prints pass or fail and hashes, never
   copies the saves, for each of the owner's `us/` and `eu/` saves: native ->
   canonical -> native identity, and the converted file loads on the other
   region's build.
5. **OLED**: a US save loaded on the EU build and an EU save loaded on the US
   build; an EU extra-chapter save shows the error 8 text on US and survives
   untouched.

## Not verified

- No EU save was read. The EU layout and the version byte come from the decomp.
- Whether the `TEXT_AREA_NAMES` / `TEXT_SHOP_NAMES` string order is the same in
  the US and EU text files (they are assets, not in the decomp). It matters only
  for the slot summary shown in the picker; check with the two discs.
- The default (new game) player and partner names in the EU languages.
- Whether the US font covers the EU Japanese-language names (hard case 5).
- The `name_matches` language-switch behaviour on the EU build.
- Anything else the EU extra chapter writes outside the 52 flags found; the
  round trip on a real EU extra-chapter save will show it.

## Order

1. Converter and tests (PC only, no game change): `save_convert.c`, the layout
   table, the unit tests.
2. Store in `memcard_host` behind a switch, off by default; import; host tests.
3. Turn it on for the separate US and EU builds in a preview (shared
   `saves/shared/`), OLED check of a cross-region load.
4. Then [P6.26](PER_USER_SAVES.md) moves `shared/` under a Switch user, and
   [P6.24](SINGLE_BUILD.md) picks it up unchanged.

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

**Findings.** The card image (header, icons, info at 0x200, three slots of
0x2700 at 0x300) and the first **9,796 bytes** of `GameState` (0x0000-0x2643:
name, money, items, cards, decks, partners, `progress`, ...) are identical.
The differences: the **flag area from 0x2644** (13 bitset groups: 4 grow, +10
bytes; 1 shrinks, -2; 8 keep their size and move; `GAME_SAVE_SIZE` 0x26BC US,
0x26C4 EU), the **version byte (3 US, 4 EU)** and the XOR **checksum** of each
slot, and the language-dependent header title and PS1 product name. The game
rejects a foreign slot with error 8. Flag ids: **544 distinct in US, 596 in
EU; every US flag is in EU and the 52 extra are the EU extra chapter**; 571 of
572 shared source files use identical flag references. Item, tech, Digimon and
card ids, stage modes and play time carry over. `LANGUAGE` is not saved.
**Hard cases:** an EU save in its extra chapter (`progress` 0x2D, set at the
end of event 1500, `wstag780.c`) has no meaning in US (proposed: error 8, bytes
kept); US flags40 bytes 0x1A-0x1B are dropped by EU (unused by any script;
kept in metadata); 26 of 230 name glyph images differ (accented EU letters,
cosmetic); the port's `name_matches` ties a save to the language's PS1
product name (an existing EU limitation, to confirm).
**Design.** A port-side container (`.dw3s`: magic, format version, crc, origin
region and language per slot) holding a card image with the **EU layout as the
canonical data**; pure converters at the `memcard_host` boundary (open: canonical
-> the running region's native image; flush: back, only the slots the game
wrote), so the game keeps seeing its own format. Existing `us/memcard` and
`eu/memcard` saves are **copied** in (first-fit of the non-empty slots into the
6 slots), originals never touched, backups before overwriting a foreign slot.
