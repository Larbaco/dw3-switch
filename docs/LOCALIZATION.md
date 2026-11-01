# Localization (P6.6, P6.7 and P6.8): PT-BR first

Status: **design and extraction tooling, written 2026-11-01** (phase 0, this PR). No
behaviour change in the game or in the Options yet. Owner decision, 2026-11-01:
**translation is the priority: Brazilian Portuguese for the game and for the port's
own screens.** Roadmap: [P6.6](ROADMAP.md) (this note), [P6.7](ROADMAP.md) (the
PT-BR pack), [P6.8](ROADMAP.md) (more locales, the same mechanism).

Everything below was measured on the owner's own EU and US discs with
`tools/l10n/extract_text.py` and the decomp. The numbers are in section 2.8; the
disc text itself is never in this repository.

## 1. Approach and the legal line

| What | Where it lives | In git? |
|---|---|---|
| The **game's text** in PT-BR (dialogue, names, menus, descriptions) | A **local pack** built on the owner's PC from the owner's own disc and the owner's translation: `runtime/<region>/l10n/ptbr/` (on the Switch `sdmc:/switch/dw3/<region>/l10n/ptbr/`) | **No. Never committed, never shipped, never in a release.** |
| The extracted **template** and the translation work files | `tmp/l10n/ptbr/` (git-ignored) or any path outside the repo | **No.** The tool refuses to write inside the repo unless the path is git-ignored |
| The **extraction, check and pack tools**, the loader, the format description, the glossary rules | `tools/l10n/`, `port/l10n/`, this note | Yes |
| The **port's own text**: Options, Help, Walkthrough, the port's messages (quit, restart, region, cheats) | `port/help/text/pt.txt`, `port/walkthrough/text/pt.txt`, `tools/l10n/ptbr_options.py` | **Yes, in PT-BR, our own writing** |
| Font glyphs the game's font does not have | An **owner-built atlas** (glyph cells drawn locally from a free font, DejaVu, which is already a dependency), written into the pack folder | The recipe yes, the atlas no (it is built on the PC) |

Rules that follow from [AGENTS.md](../AGENTS.md):

1. A PT-BR translation of the game's lines is a derivative of the game's text. It is
   built **locally** and used locally, exactly like the extracted disc tree.
   Releases, issues and PRs never carry it; screenshots of translated game text stay
   in `tmp/evidence` (AGENTS rule 3 of the owner's machine rules).
2. Everything the port writes itself is ours and may be committed in PT-BR. It must be
   **our own wording** from the decomp, the repo docs and our own testing. The
   copyright rule applies: **never GameFAQs or any external guide text**, and no text
   taken from another game's translation.
3. The owner's `tools/guide_walkthrough_ptbr.json` (a translation of a third-party
   guide, used only by the local guide HTML generator) **must not be copied into the
   game or into the port's PT-BR text**, and its wording is not a source for the
   glossary. See section 6.4.
4. Rule 8 (every change versus the original game is an Options row): the language
   choice is one (section 3.6).
5. No gettext-style layer, no host (`platforms/*`) code except the homebrew chrome:
   the work is a remap and charset glue in `port/`, as P6.6 asked.

## 2. How the game text works

### 2.1 Language slots

The US disc has one English copy of every text file. The **EU disc has the text
files once per language, in a row of seven**, and the engine picks the copy with
`LANGUAGE` (`vendor/dw3_decomp/include/engine/file.h`):

```c
#define TEXT_FILE(file) (LANGUAGE + (file) - 1)            /* EU */
#define TEXT_ENTRY(file, n) ((LANGUAGE << 16) + ((file) << 16 | (n)))
```

The row of one text file on the EU disc (from `files_eu.json`, file `AMTMAP` shown):

| LANGUAGE | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| directory | `JPN` | `USA` | `ENG` | `FRA` | `ITA` | `GER` | `SPN` |
| file id of AMTMAP | 1 | 2 | **3** | 4 | 5 | 6 | 7 |

The country select screen (`CNTY_SEL_languages = {2, 3, 5, 4, 6, 1, 0}`) offers EN,
FR, DE, IT, ES (LANGUAGE 2, 3, 5, 4, 6); 1 and 0 (US and Japanese copies) are on the
disc for the stage-select debug and are not reachable in normal play. The five
languages have **the same number of strings in every file** (13,084 on EU), and the same
index is the same sentence: the page, wait, speaker and work-buffer codes agree in
99.8% of the 52,336 string comparisons against English (104 differ). That alignment is
what makes keys, fallback and limits possible.

Other `LANGUAGE`-dependent things (all in the decomp): the title logo bank
`STDWTITL_titleImages[LANGUAGE]` and the three title menu labels
`STDWTITL_menuSprites[LANGUAGE]` (graphics), the name entry word sprites
(`NAME_ENTRY_TEXT_SPRITE`), the card battle AP/HP bar labels
(`CARDGAME_panelBarSprites[LANGUAGE]`), the splash frame, the movie choice, the save
file name (`setSaveFileName`, the same for all EU languages) and the field text file
(`FIELDSTG_state.textFile = LANGUAGE + 0xE8`, a stage's `STAGE_TEXT = LANGUAGE + 0xEF`
and the event text `EVENT_TEXT_FILE + LANGUAGE`).

### 2.2 Containers and the string format

All text files are `FILE_CACHE` loads of one of two containers (`getString` in
`text_tools.c`; both parsed by `tools/l10n/textfmt.py` and rebuilt without loss):

* **table**: `u32 count`, then `count` `u32` offsets from the start of the table,
  then the strings, each ended by a 0 byte and 4-aligned (39 of the 50 EN files).
* **archive** of tables (the 11 `DMG###` files, the event text of a stage group): `u32`
  offsets of tables (the first is also the header size), each table with its own offsets.

Strings are font-code byte strings (`decodeChar` in `font.c`, `text_window.c`):

| Byte | Meaning |
|---|---|
| `00` | end of the string |
| `01 n` | icon `n` (1..114) of the style's icon sheet: **space, punctuation, button symbols and, on EU, every accented letter** |
| `02 c args` | control code `c`; total length 1, 2, 3, 2, 5, 3, 3, 2, 3, 2 for `c` = 0..9 |
| `03` | a missing character (draws glyph 0) |
| `04`..`E9` | glyph `n - 4` of the sheet: digits `04`..`0D`, `A`..`Z` `0E`..`27`, `a`..`z` `28`..`41`, then Japanese kana, with `?` = `E6` and `!` = `E7` |

Control codes: `1` new line, `2 a` wait for a button (`a` 1..4 picks which), `3` page
break, `5 n` insert work buffer `n` (an item or Digimon name put into a sentence),
`6 n` pause `n` frames, `7` the text starts here (after a speaker's name), `8 n`
put the player's name into work buffer `n`, `9` shows the page like `0` (850 EN
strings carry `02 09`, right after the speaker). A line is ended only by the explicit `02 01`; **the engine
never wraps text**, so line breaks are part of the translation.

The editable form used by the tools is plain text and round-trips every byte (proved
on every string of all five EU languages, the US row of the EU disc, the Japanese row and the US disc, 0 errors; three 512-byte placeholders in the EU disc's US row are not text and are skipped):
letters as themselves, a new line as `\n`, and tokens `{pg}` `{start}` `{wait:N}`
`{buf:N}` `{pause:N}` `{name:N}` `{raw:0209}` (code 9; copy it unchanged),
`{i:N}` and `{g:N}` for icons and glyphs that are not text (button symbols, kana),
`{x}`. Example (made up, not game text): `{start}Olá, {buf:1}!\nVamos?{wait:2}{pg}`.

### 2.3 The font: do the accents exist?

**Yes. The EU font carries every letter PT-BR needs, lower and upper case**, as
icons of the 114-icon sheet (EU icons 59..114 are Latin letters where the US sheet and
the Japanese disc have kanji). They were identified from the glyph art
(`tools/dump_font_cells.py`-style render, checked cell by cell) and confirmed against
the French, Spanish, Italian and German text, where the same indices carry `é`, `ñ`,
`ä`, `ì`, and so on:

| | á | à | â | ã | é | ê | í | ó | ô | õ | ú | ç | ü |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| icon | 95 | 75 | 76 | 77 | 81 | 82 | 96 | 97 | 89 | 90 | 98 | 79 | 94 |
| | Á | À | Â | Ã | É | Ê | Í | Ó | Ô | Õ | Ú | Ç | Ü |
| icon | 104 | 109 | 99 | 64 | 105 | 100 | 106 | 107 | 101 | 70 | 108 | 68 | 73 |

`ã`, `õ`, `Ã`, `Õ` and the capitals `Â Ê Ô` are **not used by any EU text** (Portuguese
is the only language that needs them), so they are verified by their glyph shape, not
by usage: the first on-screen test (phase 2, row L3) prints all 26 on one screen. `¡`
(59), `¿` (63) and `°` (114, usable as `º`) also exist; `ª` and `º` are not separate
glyphs. The Latin-1 table `EU_ICONS` in `tools/l10n/textfmt.py` is the source of the
mapping; `PTBR_ACCENTS` is the set PT-BR needs and a unit test proves it is all
encodable on EU and not on US.

Glyph metrics (`FONT_GLYPHS_n` / `FONT_ICONS_n`, three sizes: lines of 14, 11 and 9
px) give every character its `advance + dx`; the font is **proportional** (letters
5 to 7 px, accents the same as their base letter, space 6). The accent sits in the top
rows of a 12 px cell, so stacked lines at 14 px do not collide.

**US:** the US font has no accents (the same icon slots hold kanji art). PT-BR on the
US disc needs the owner-built glyph atlas (section 3.5). The EU disc needs none.

The port's own screens do not use this font: they draw with DejaVu Sans baked into
`port/pages/help_atlas.inc` (Latin-1, so already complete for PT-BR) and
`platforms/pc/src/title_overlay_lettering.inc` (one baked bitmap per string).

### 2.4 Line widths and box limits

The text engine measures with `measureText` (sum of `advance + dx`, longest line).
Boxes: the message box is a text window at (18, 176) with `setLines(3)`; talk boxes
(`createTalkBox`, 4 types) have a speaker window and a 3-line message window; all use
style 1 (14 px lines) unless a menu picks style 2 or 3. The widest line of any string
in any of the five EU languages (style 1) is **261 px** (a memory card message); dialogue
and event lines reach 252 px, descriptions 239 px, and **names stay under 160 px**
(Digimon 97, items 93, techniques 101, cards 104, stages 115, areas 97, shops 129,
inns 160 px).

Spanish, French, Italian and German were fitted to the same boxes as English by their
translators (total width of the dialogue is within +6% / -15% of English, and about
1 line in 100 needed one more line on the page), which is what PT-BR will need too
(PT is close to ES and FR in length). So the **limit of a string is data, not a guess**:
for every key `extract_text.py` stores `limit_px` (the widest line of the five EU
languages at the same index, style 1) and `max_lines`, and `--check` warns when the
PT line is wider than every other language and errors on more lines per page than the
box has. The absolute box widths per call site are measured on screen in phase 2 (row
L5 to L7).

### 2.5 Names in battle and menus

Names are ordinary strings of dedicated tables, indexed by item, Digimon or
technique id: `DIGNAM` (251 Digimon), `ITMNAM` (403 items), `SKLNAM` (444
techniques), `CARDNM` (317 cards), `SHPNAM`, `HTLNAM`, `STNAME`, `STAREA`.
Descriptions are the parallel `DIGINF`, `ITMINF`, `SKLINF`, `CARDST`. Names are put
into sentences with `02 05 n` (work buffers); **188 EN strings use a buffer and 122 use
the player's name**, so a sentence must stay valid for any name that can fill the
slot: no gender or number agreement that depends on the inserted word ("o item" /
"a item"), rephrase to a neutral form ("Item: {buf:1}"). The Digimon and item names
shown in battle windows are the same strings; the windows are sized for the longest
EU name, which `limit_px` already encodes.

### 2.6 Variable-length strings and pointer tables

Every string is reached through an offset table that is part of the file, so **a PT
string may be any length**: the pack builder writes new tables with new offsets and
the engine needs no change. Two things do depend on the file size: the engine reads
`FILE_SECTOR_COUNTS[file]` sectors from `FILE_SECTORS[file]`
(`file_table.c`, `FILE_SECTOR_COUNTS` is an exported `u16` array the port can patch),
and `FILE_CACHE` allocates that many sectors from the heap. The biggest EN text file
is 48 KB (`TALK07`); PT is expected at +0..+10% over EN. Phase 1 proves the heap high
water with `tools/memaudit`. The original files share equal strings and pad
differently, so a rebuilt file is not byte-identical to the original (29 of 50 are
not); this does not matter, because the pack builder checks by decoding.

### 2.7 Hard-coded strings and text that is a picture

* **In code:** almost none. The decomp's string literals are asset names, the save file
  names, and a handful of Japanese debug strings (`STR_NULL_MESSAGE` and three more, shown
  only on an engine error, and the stage-select debug menu). **Nothing the player
  reads is hard-coded in the C code**, so no code string needs translation. The player
  name keyboard is a table of glyph indices, not text; its keys have no accents (typed
  names stay unaccented, accepted limitation, phase 4 may add accent keys).
* **As graphics (the game's own art, not strings):** the three title menu labels per
  language (PRESS START, New Game, Continue; the port already draws the EU ones
  from the captured sprites and its own lettering for the Options row), the title
  logo bank (shared by all five EU languages), the name entry words, the card battle
  AP/HP bars, the splash. PT-BR has no original art for these: phase 4 draws them with
  the port's own DejaVu lettering (3 title labels, 2 bar labels, the name entry words), and
  falls back to the English art before that.
* **Movies** (`.STR`) are out of scope: the decomp picks one of three by `LANGUAGE`
  (Japanese, US, and one for all five EU languages), so PT-BR keeps the English-slot movie.

### 2.8 How big is the translation

EU English (the base; the 50 files of the `ENG` row), counted by `extract_text.py --stats`:

| Category | Files | Strings | Non-empty | Unique | Words | Words, unique strings only |
|---|---|---|---|---|---|---|
| names (`DIGNAM ITMNAM SKLNAM CARDNM SHPNAM HTLNAM STNAME STAREA`) | 8 | 1,663 | 1,655 | 1,145 | 2,635 | 2,075 |
| descriptions (`DIGINF ITMINF SKLINF CARDST`) | 4 | 918 | 914 | 628 | 3,454 | 2,996 |
| menus (17 files: `STATUS MFIGHT CARDGM TRAINI ...`) | 17 | 869 | 852 | 696 | 2,112 | 1,876 |
| dialogue, NPC talk (`TALK00..09`) | 10 | 7,383 | 7,373 | 2,832 | 48,310 | 40,661 |
| dialogue, events (`DMG200..900`, 58 sub-tables in the first) | 11 | 2,251 | 1,858 | 1,660 | 21,194 | 19,895 |
| **Total** | **50** | **13,084** | **12,652** | **6,901** | **77,705** | **67,363** |

About 407,000 characters. **One translation of the 6,901 unique strings (67.4k
words) covers all 12,652.** The first three categories (names, descriptions, menus:
2,469 unique strings, 6,947 words) are what the player sees most and are a small
first milestone; the dialogue is 90% of the work.

The **US English text is the same text**: 12,470 of its 12,542 strings are identical to
the EU string with the same key (file stem and index), only 55 unique US strings
(260 words) do not exist on EU, and the US disc lacks `TALK08`, `TALK09` and `DMG900` (text of
the EU-only content). So the PT pack is keyed by `STEM:index` and **serves both regions
from one translation**; the extractor writes the US template too, and `--propagate`
fills the US from the EU translation by identical English.

Strings with game markup: 4,866 with line breaks, 4,964 with a button wait, 2,750 with
a page break, 188 with a work buffer, 122 with the player's name.

## 3. Pack design

### 3.1 Which slot: the decision

| Option | Idea | Verdict |
|---|---|---|
| A. **Replace** an EU slot (e.g. FRA) with PT | Write the PT files over `FSxxxx.BIN` | Simple, but it destroys that language, cannot be switched back from Options, and rewrites the owner's runtime. **No.** |
| B. A **new `LANGUAGE` id** (7) through the `port/cdfs` remap | `TEXT_FILE(file)` for 7 lands on the next file's JPN copy; every `[LANGUAGE]` table (title banks, 7-row sprite tables, movies, splash, card bars) is sized for 0..6 | Invasive: a patch per table, and a PT slot with no art. Keep for **P6.8** as the generalization (section 3.7), not for the first language. |
| C. **The English slot plus a text overlay** (`language=ptbr`) | The engine stays at `LANGUAGE = 2` (English), so every table, bank, sprite and movie that depends on it keeps working. When the option is on, the port serves the PT files in place of the English text files | **Chosen.** It also matches the US (no `LANGUAGE`): the same remap serves both. |

Under C the EU **country select screen is unchanged**. `Português (BR)` is a text
overlay of the English slot, selected in Options (section 3.6): with the row on, a
player who picks English at the country select gets PT text; the other four
languages are never overlaid.

### 3.2 Pack format

A pack is a folder, built by `tools/l10n/build_pack.py` (phase 1), never committed:

```
runtime/eu/l10n/ptbr/
  pack.json        format, region, language "ptbr", counts, sha256 of every source file
  <fileid>.bin     one finished text file per English file id (50 on EU), same container
  glyphs.bin       the glyph atlas (US, section 3.5; absent on EU)
  glyphs.png       preview of the same, for the owner
```

* The **translation source** is the template JSON of section 4 (`tmp/l10n/ptbr/<region>/<STEM>.json`,
  UTF-8, one file per game file, each entry `{key, en, pt, limit_px, max_lines}`
  keyed by `STEM:index` or, for an event archive, `STEM:table:index`).
* **Fallback per missing string:** an entry with an empty `pt` keeps the original
  English bytes, so a half-finished translation is playable. The builder logs the
  count; the loader needs no per-string logic.
* `pack.json` records the sha256 of the **source English file** each file was built
  from; the loader refuses a pack whose source hash does not match the runtime's file
  (a pack built against another disc dump), with a log line, and plays English.

### 3.3 Loader (`port/l10n/`, phase 1)

1. At boot, after the cdfs map loads, `dw3_l10n_init(region, root)` reads
   `<root>/l10n/ptbr/pack.json` if the option `text_language` is `ptbr` (and, for a
   test, `DW3_LANG=ptbr`). It validates the hashes and the sizes.
2. It adds each pack file to the cdfs map as an **extra entry with a virtual LBA**
   above the end of the disc and, only while the option is on, rewrites
   `FILE_SECTORS[id]` and `FILE_SECTOR_COUNTS[id]` for the 50 English text ids to the
   pack entry (the engine reads those two arrays, `file_table.c`; the port already
   reaches them through `dw3_game_api`). No engine patch is needed, a PT file may
   exceed the English one, and switching back restores the two arrays.
3. Text files already in `FILE_CACHE` are evicted when the option changes, so the
   change is seen at the next load (title screen and field map loads read them anew).
4. Anything wrong (no pack, hash mismatch, short file) leaves the arrays untouched:
   **English, with one log line**. The game never fails because of a pack.

The US build uses the same loader, with ids from `files_us.json` (no `LANGUAGE` offset).
How the engine's sector reads reach a virtual LBA (the PSY-Z `libcd` backend maps a
sector number to a cdfs entry) is the first thing the phase 1 spike proves; the fall-back
if it cannot is an id-level override in the cdfs read path, with the same two arrays
patched.

### 3.4 Font and accents: EU

None needed; section 2.3. The encoder writes accents as `01 n` icon pairs. A unit test
(`tools/tests/test_l10n_extract.py`) proves the PT-BR set is encodable.

### 3.5 Font additions: the owner-built atlas (US, and anything missing)

For the US disc (no accents) and for any character the EU font lacks (for example
`ª`, typographic quotes, an `ü` in a name), `build_pack.py --atlas` renders the
missing cells from DejaVu Sans (12 px cells, the same advance rule), writes them to
`glyphs.bin`, and records `icon -> cell`. At pack load the port copies the cells into
free VRAM and rewrites the matching `FONT_ICONS_n` entries (the table is data in the
engine's `.data`; the unused kanji slots 59..114 are the targets, so no new index
space is needed). This is the technique of the P10.6 Switch glyph atlas
([P10.6_GLYPHS](P10.6_GLYPHS.md), `port/options/switch_glyphs.c`): after `loadFont()`
the cells are put in free VRAM and the `FONT_ICONS_k` entries are rewritten. The cells are **our own drawing** (a free font
rendered locally), not a copy of the game's art, and they stay in the pack folder.

### 3.6 Options row, selection and logs

* A **new Options row**, "Language: Original / Português (BR)" (key `text_language`
  in `port_options.cfg`, values `original|ptbr`, default `original`), with its label
  and message line in the five languages **and PT-BR** (AGENTS.md rule 8; not on the
  exempt list). Recommended place: the first row of a one-row **Language** group at the
  top of the group list (an eleventh group; owner to confirm, the alternative is the
  first row of Display).
* **EU and the combined image** (US+EU build, `docs/SINGLE_BUILD.md`): the row is on
  both regions, the same cfg key (one global `port_options.cfg`, P6.26). A region with
  no installed pack shows the row greyed with the reason ("pack not found") and the
  log line below.
* **US gets it too**, in phase 5, because the translation is shared (section 2.8) and
  the only extra work is the atlas (3.5). Until then the US row shows "needs the EU
  font" greyed.
* Switching applies at the next text load; the Options screen itself changes
  language at once (section 7), since it does not use the game's text.
* **Logs** (once per event, never per frame; AGENTS rule 12):

```
dw3-l10n: option text_language=ptbr region=eu
dw3-l10n: pack ok region=eu files=50 strings=13084 translated=N fallback=M src=sha256:...
dw3-l10n: override id=108 stem=ITMNAM bytes=7810 lba=0x100006e sectors=4   (first 8, then a count)
dw3-l10n: fallback key=TALK03:0412 (empty pt)                              (first 32, then a count)
dw3-l10n: off reason=no_pack | hash_mismatch | short_file | option_original
dw3-l10n: heap high water text=NNNN
```

### 3.7 Tests

* **Unit (this PR, synthetic data only):** `tools/tests/test_l10n_extract.py`
  covers the container parser, the codec (accents on EU, rejected on US), widths,
  extraction, every `--check` rule, `--against`, `--propagate`, the glossary check
  and the refusal to write into git.
* **Phase 1:** `test_l10n_pack.py` (builder on synthetic text files: fallback, hashes,
  alignment, archives), `l10n_main.c` (loader on a synthetic pack: arrays patched and
  restored, hash refusal, short file), a no-game-text guard (no tracked file may contain
  the template format marker), cfg tests for `text_language`.
* **Replays (owner's runtime, local refs only):** a **synthetic pack made from the
  EN text with a visible transform** (every string upper-cased with `ã õ ç` added) so
  no translation is needed: `eu-l10n-title`, `eu-l10n-field-dialogue`,
  `eu-l10n-battle-menu`, `eu-l10n-fallback`, `us-l10n-*` in phase 5. They prove the
  loader and the glyphs, on EU first, then both regions for the gate.

## 4. Extraction tool (built in this PR)

`tools/l10n/extract_text.py` (library: `tools/l10n/textfmt.py`).

```
python3 tools/l10n/extract_text.py                          # writes tmp/l10n/ptbr/{eu,us}/
python3 tools/l10n/extract_text.py --region eu --refs fr,de,it,es --out ~/l10n/ptbr
python3 tools/l10n/extract_text.py --stats                  # only the counts of section 2.8
python3 tools/l10n/extract_text.py --check tmp/l10n/ptbr/eu [--strict] [--against FRESH_DIR]
python3 tools/l10n/extract_text.py --check FILE.json --glossary glossary.json --max-width 270
python3 tools/l10n/extract_text.py --propagate tmp/l10n/ptbr/us tmp/l10n/ptbr/eu
```

It reads `~/.cache/dw3-switch/runtime/{eu,us}` (`--runtime` to change it), needs no
Python package beyond the standard library, and refuses to write inside the repository
unless the path is git-ignored. `tmp/` is ignored. **Template** (one JSON per game file,
`format: dw3-l10n-v1`): `region`, `stem`, `file_id`, `container`, `category`, then
entries `{key, en, pt: "", limit_px, max_lines}`, plus `fr/de/it/es` references with
`--refs` (handy for an AI translator: four professional translations of the same
sentence; local only).

`--check` (needs no disc; exit 1 on errors) validates, per translated entry:

| Check | Level |
|---|---|
| every character can be written with the region's font (EU: Latin-1 accents; US: none) | error |
| the control tokens (`{wait}`, `{pg}`, `{start}`, `{buf}`, `{name}`, `{raw}`, icons) equal the English ones (new lines may move) | error |
| lines per page <= `max_lines` (3 for dialogue) | error |
| a line wider than every other EU language (`limit_px`) | warning |
| absolute width with `--max-width` | error |
| work-buffer placeholders in a different order | warning |
| glossary term of the English missing in the PT (`--glossary`) | warning |
| duplicate keys, bad JSON, unknown token | error |
| `--against`: missing, unknown key or changed English (a template from another disc) | error |
| `--strict`: warnings and untranslated entries count as errors | |

An empty `pt` is "not translated yet" (kept in English by the builder). Proven locally on
all 12,652 + 12,119 strings: an identity translation (`pt = en`) passes with 0 errors
and 0 warnings against a fresh template.

## 5. Translation workflow

1. `extract_text.py` writes the template (section 4). Nothing from it is committed.
2. **Who translates:** the owner (native speaker, final say), **assisted by an AI
   running locally** on the owner's PC with the template as input. The AI sees the
   English line, the four other EU translations (`--refs`), `limit_px`, `max_lines`
   and the glossary, and fills `pt`. No cloud service is given the game text beyond
   what the owner chooses to send; the repo never stores it.
3. `--propagate`, then `--check` after every batch; `--check --strict` before a pack
   is called finished. `build_pack.py` (phase 1) turns the folder into the pack.
4. Order: **names, menus and descriptions first** (2,469 unique strings), then
   `TALK00..09` and the `DMG` events chapter by chapter, because the build is playable
   at every step (empty `pt` = English).
5. Review passes in the game: the owner plays; a string that looks wrong is fixed
   in the template by its key, which the debug line prints (phase 2 adds
   `DW3_L10N_SHOW_KEYS=1`).

### 5.1 Glossary rules

* **Official PT-BR Digimon names where an official Brazilian one exists, otherwise keep
  the original name** (the Digimon, item and technique names as the EU English
  disc shows them). A name is never translated by sense when it is a proper noun
  (places such as Asuka City, Seiryu City, Digimon Online stay).
* The glossary file (`glossary.json`, `{ "English term": "PT-BR term" }`, outside git
  with the translation) is **built by the owner from the disc's own tables and from
  official Brazilian releases they own**, not from a third-party guide.
  `--check --glossary` flags a line that has the English term but not the PT one.
* One term, one translation, everywhere (HP, AP, Digi-Egg, Partner, Tamer, Stage names).
  Game terms that PT-BR gamers use in English may stay (HP, AP) if the owner decides
  once.

### 5.2 Style guide (short)

* Brazilian Portuguese, **informal "você"** (the game is aimed at young players; never
  "tu", never European forms); present tense; same register as the English line.
* Keep every `{token}` and the box structure: the number of pages and `{wait}` stays.
  Re-wrap with `\n` to fit `limit_px` and 3 lines; rephrase before abbreviating.
* Names inserted by `{buf:N}`: write sentences that work for any item or Digimon, no
  agreement with the inserted word (section 2.5).
* Exclamation and question marks are written normally (`!` `?`); inverted marks are
  not used in Portuguese.
* Numbers, `HP`, `AP`, key names and button names as in the English line.
* Do not translate speaker names that are Digimon names; translate roles ("Guard").

### 5.3 The owner's PT-BR guide (`tools/guide_walkthrough_ptbr.json`)

That file is the owner's translation of a third-party (GameFAQs) guide for the local
guide HTML tool (`docs/GUIDE_HTML.md`). It is **a separate project and must not feed the
game or the port**: not copied into `pack`, `port/walkthrough/text/pt.txt`,
`port/help/text/pt.txt` or the glossary, and not used as a style reference. The port's
Walkthrough text is our own writing from the decomp's stage scripts and flags
(`docs/P6.35_WALKTHROUGH.md`), and its PT-BR is a translation of **our** English
text. Once the game pack exists, the walkthrough's place and item names are aligned to
the pack's glossary (a find and replace in `pt.txt`), still from the game's own text.

## 6. The port's own screens in PT-BR (drafted in this PR)

The port's screens read no game text, so they are finished in the repo, flagged for
**owner review** (a Brazilian reader):

| Screen | Source | PT-BR | Status |
|---|---|---|---|
| Help (P6.30) | `port/help/text/en.txt` (221 lines) | `port/help/text/pt.txt` | drafted in this PR; `tools/generate_help_assets.py` validates it (keys, tokens, widths, atlas glyphs), `--with-pt` embeds it |
| Walkthrough (P6.35) | `port/walkthrough/text/en.txt` (353 lines) | `port/walkthrough/text/pt.txt` | drafted; budgets and keys checked by `test_ptbr_port_text.py`; `tools/generate_walkthrough_text.py --with-pt` embeds it |
| Options, Cheats, quit, restart, region, locked row, title labels | the `_en` entries of `tools/generate_pc_title_lettering.py` | `tools/l10n/ptbr_options.py` (one PT string per English entry, 177 of them) | drafted; `generate_pc_title_lettering.py --check-pt` checks coverage and the same pixel limits as the five languages; `--with-pt` bakes them |

All three generators gain `pt` **without changing their default output**: the committed
`.inc` files are byte-identical, so nothing in the binary or the replays moves. The C
side (language index 5, the `pt` Options lettering and text tables) is wired in
phase 3. FR/DE/IT/ES of the same screens remain machine-drafted pending native review
(P6.30); the PT-BR draft is in the same state, **drafted by an AI, owner review
pending**, and its `@status` says so.

## 7. Phased build plan

Efforts are worker-days (one agent, with the build and replay time included).

| Phase | Work | Effort | Done when |
|---|---|---|---|
| **0** (this PR) | Design note, `extract_text.py` + `textfmt.py` + tests, `pt` drafts of Help, Walkthrough and Options lettering, `--check-pt` / `--with-pt` generators | done | the PR merges; the owner reviews the PT drafts |
| **1** Pack and loader (EU, PC) | `build_pack.py` (+ synthetic tests), `port/l10n/` loader (virtual LBA entries, `FILE_SECTORS` / `COUNTS` patch and restore, hash check, logs), `text_language` cfg key (no UI yet, `DW3_LANG=ptbr` for tests), synthetic-pack replays on EU, heap high-water | 4 | a synthetic upper-cased pack shows in the title, field dialogue, battle menu and Digimon lab on EU PC, and removing the pack gives English with one log line |
| **2** Options row, glyph proof, boxes | the Language row (5 languages + PT) in the registry and the lettering, the on-screen accent proof (all 26 letters, `¡ ¿ °`), a measuring pass of every window (message box, talk box types, battle menu, field menu, shop, card game) with the longest PT-like strings, `DW3_L10N_SHOW_KEYS=1`; Switch build and a first OLED run | 3 | OLED L1 to L7 pass with the synthetic pack on EU |
| **3** Port screens PT | `pt` index 5 in the Options lettering table and the Help / Walkthrough embeds (generators with `--with-pt` on by default), the language follows the Language row, title Options plaque and Quit in PT, replays of the Options / Help / Walkthrough in PT on EU and US | 3 | OLED L9 and L10; the owner signs off the PT drafts |
| **4** Translation, first pass | Owner + local AI: names, menus, descriptions (2,469 strings), then `TALK00..09` and `DMG` by chapter (the 90%); check and pack after each batch; PT title labels and bar labels drawn with our lettering | owner/AI time, not worker time: about 3 days of AI drafting and 25 to 40 h of owner review for 67k words | a short playthrough title to field dialogue in PT-BR (the P6.7 done-when) |
| **5** US | The owner-built atlas (3.5), the US template filled by `--propagate`, the 72 differing US strings, US replays, the combined-image row | 4 | the same playthrough on a US disc |
| **6** P6.8: a second language | `build_pack.py --lang <code>`, `l10n/<code>/` folders, a `languages` list in the Language row, country-select integration, and the decision of option B (a real `LANGUAGE` id) only if a language needs its own art | 4 | a second owner-supplied language switches and renders |

Risks: the heap (`FILE_CACHE` sectors for longer files; phase 1 measures it), glyph
shape of `Ã Õ Ê Ô` (checked on screen in phase 2), a window whose box is narrower than
the EU maximum of 261 px (phase 2 measures them), grammar around work buffers.
Open owner questions: where the Language row goes (3.6), the keyboard accents
(2.7), and whether the title labels use the port's lettering or stay English.

## 8. OLED rows (owner checklist, from phase 2)

Every row names the `dw3.log` line that proves it (AGENTS rule 12). Rows L1 to L8 use
the synthetic pack, so they need no translation.

| # | Steps | Expected | Proof in `dw3.log` |
|---|---|---|---|
| L1 | EU build, no pack installed. Options, Language | The row exists, shows Original, and the PT value is greyed ("pack not found") | `dw3-l10n: off reason=no_pack` |
| L2 | Install the synthetic pack on the SD card, restart | The row can be set to Português (BR) | `dw3-l10n: pack ok region=eu files=50 ... fallback=` and `option text_language=ptbr` |
| L3 | The accent proof screen | All 26 accents, `¡ ¿ °` visible and legible handheld and docked (`Ã Õ Ê Ô` top rows not clipped) | `dw3-l10n: glyph proof n=29` |
| L4 | New game, name entry, the first field dialogue | PT text in the box, no letter clipped, 3 lines per page | `dw3-l10n: override id=...` lines, no `fallback` for the visited file |
| L5 | Field menu: Items, Techniques, Status, Map | PT names and descriptions fit their windows | `dw3-l10n: override` for ITMNAM, SKLNAM, STATUS |
| L6 | A battle: command menu, a technique, the damage text, the report | Everything fits, no overlap | `dw3-l10n: heap high water` |
| L7 | A shop, the inn, the Digimon lab, the training gym, the card game | Menus fit | same |
| L8 | Pack with deleted strings | The missing lines show in English | `dw3-l10n: fallback key=...` |
| L9 | Switch the row back to Original, play on, save, switch again | English at once; saves are unaffected | `dw3-l10n: off reason=option_original` |
| L10 | Options, Help, Walkthrough in PT-BR (phase 3) | Texts fit, no clipped accents; drafts approved | no log line (visual) |
| L11 | Delete the pack with the game closed, start | English, no error | `dw3-l10n: off reason=no_pack` |
| L12 | Timing: the title to field load times with and without the pack | No extra wait | `dw3-l10n:` timing line |
| L13 | US build (phase 5): L2, L4, L5 | Same as EU | same, `region=us` |

## 9. As built

Phase 0 (2026-11-01): `tools/l10n/extract_text.py`, `tools/l10n/textfmt.py`,
`tools/tests/test_l10n_extract.py` (synthetic data), the note you are reading, and the
`pt` drafts of section 6. Nothing else changed; no behaviour change in the game, the
Options or any generated `.inc`.
