# Local GameFAQs guide HTML

`tools/guide_to_html.py` converts the locally saved GameFAQs HTML page (even
when saved with a `.txt` extension) into an offline chapter package (recommended)
or a compatible single HTML file. Python 3 and its standard library suffice:

```sh
python3 -I tools/guide_to_html.py /path/to/saved-page.txt /mnt/cabal/tmp/dw3-guide --format site
# Copy the whole folder and open index.html. No server or internet is needed.
python3 -I tools/guide_to_html.py /path/to/saved-page.txt /mnt/cabal/tmp/guide.html --format single
TMPDIR=/mnt/cabal/tmp python3 -B -m unittest discover -s tools/tests -p 'test_guide*.py' -v
```

Keep the source and generated HTML local; neither is included in this change.
The converter retains the existing structured renderers, original-text toggles,
ASCII fallback, sidebar, search and Ctrl+K palette.

## Entity links

Names are indexed only from explicit guide data: partner/evolution chapters,
technique headings, enemy records and battle rosters, item lists and drops,
weapon/armor rows, walkthrough location headings, and booster/card lists.
A shared destination retains **all** roles and definition sources for each
normalized name. For example, a Digimon that is also an enemy or card has one
entry listing those roles; repeated partner/evolution definitions are retained.
Definition links and backlinks to rendered source blocks let readers move
between walkthrough, reference data and card lists. All entities are available
through Ctrl+K and the sidebar's entity index. Palette results use the neutral
category “Entidade” and show every indexed role (for example,
“Carta / Digimon / Inimigo”); role labels wrap instead of being truncated.

Entity IDs combine a readable slug with a digest of the normalized name;
source-block IDs use their owner and a content digest. They are deterministic
for the same source and do not depend on Python's randomized hash. Technique
card IDs share the document's ID allocator, including collisions/duplicates.
Every generation checks duplicate IDs, missing fragment targets and nested
anchors **before writing the output**.

Matching ignores case and collapses whitespace for entity identity, but does
not guess spelling corrections, abbreviations, punctuation, colors or numbered
variants. Longer documented names win. Ordinary single-word names (items,
techniques, rooms or programs) link only as exact whole text cells; prose links
require a multiword name, a Digimon-like `mon` name, or a documented booster.
Only the first occurrence per rendered fragment links. Existing anchors and
protected markup are skipped; text/attributes are escaped. Source blocks may
remain destinations when a structured renderer falls back to ASCII.

## Validation checkpoint (2026-10-24)

Using the owner's saved EmeraldPhoenix guide: 449 indexed names, 30 names
with multiple roles, 1,357 unique IDs, 5,028 internal hrefs, no missing targets
or nested anchors. All 159 original/ASCII `<pre>` blocks had identical decoded
text compared with the supplied converter. Structured coverage stayed at
96/96 partner/evolution blocks, 4/4 boosters and the enemy list; four existing
ASCII fallback blocks remain (two empty item lists and two unusual enemy lists).
Six synthetic regression tests cover ambiguity, collisions, markup/escaping,
boundaries, ASCII preservation, backlinks, integrity failures and palette
categories with all seven entity roles and multi-role destinations.

A hidden T3 browser tab at 1280x800 also checked the live DOM and followed
walkthrough → Renamon's Digimon/card entry → walkthrough. No OLED, emulator,
ISO or gameplay validation was performed: this utility does not load either
US or EU game data and does not change the port runtime.

## Brazilian Portuguese walkthrough

The Walkthrough opens in Brazilian Portuguese. Only its 19 chapters, editorial
prose, strategy paragraphs and chapter headings are localized. Official names
(Digimon, characters, places, items, techniques, organizations and recognizable
menu/game terms) retain their source spelling, including apparent source errors.
Reference sections are not translated. Tables, numeric rewards, battle rosters,
card folders, ASCII diagrams and their data retain the English source layout;
structured walkthrough labels such as “Enemies” and “Items” are localized.
Narrative/tutorial text embedded in ASCII fallbacks has a Portuguese paragraph
alongside a collapsed, unmodified ASCII copy. Table-cell annotations stay as-is.

`tools/guide_walkthrough_ptbr.json` is a separate UTF-8 editorial catalog. Each
key is the full lowercase SHA-256 of an exact English translation unit encoded
as UTF-8. Units are parsed chapter/location titles, the decoded/reflowed text of
rendered prose/strategy paragraphs, or whitespace-reflowed narrative paragraphs
inside ASCII fallbacks (selected by `ascii_prose_units`). HTML markup and entity
links are excluded from the key. Keys do not depend on line numbers, chapter
order, Python hashing, or approximate matching. Duplicate English units share
one entry. Values are plain text, escaped before entity links are rebuilt.

The entity index is built from the original source before translated display
headings are applied; existing source IDs and destinations therefore remain
stable. The sidebar, chapter chips, previous/next navigation and Ctrl+K show
Portuguese chapter titles; Ctrl+K also searches their English titles. Full-text
search indexes the Portuguese reading text; collapsed original-text disclosures
remain excluded, as in the existing search behavior. Every walkthrough source run
has a “ver texto original” disclosure and chapter titles have their own English
disclosure; location headings expose their original text in a title attribute.
The local source file is never edited or committed, and the generated HTML is
not committed.

A missing exact key leaves the unit in English with an explicit untranslated
notice. Names intentionally retained in English have catalog entries and do
not count as missing. No fuzzy lookup, automatic translation service, or
source-text rewriting runs during generation. An invalid catalog fails loudly;
missing and unused keys are counted in the generator's coverage report.
Coverage counts editorial units (including preserved proper-name headings),
not a percentage of all source characters or structured data.

Generation checks internal links and confirms every parsed walkthrough English
source run survives verbatim as decoded `<pre>` text before writing the output.
The CLI reports translated/total unique units, missing keys, occurrences and
unused catalog keys. The offline editorial QA checkpoint below is independent
of runtime localization goals P6.6/P6.7; it does not validate gameplay or either
ISO. Do not run a game test suite for this editorial conversion.

### PT-BR editorial QA checkpoint (2026-10-24)

Three independent translator batches covered chapters 1–3, 4–10 and 11–19,
with full chapter-by-chapter self-review, followed by separate bilingual
coverage/name/number checks and a risk-based editorial sample in every chapter.
That independent sample included all 43 ASCII narrative/tutorial units and both
intro paragraphs; four grammatical-agreement corrections were reconciled.
The directions preface and 43 narrative/tutorial paragraphs embedded in ASCII
fallbacks are included. The catalog covers **623/623 unique units**, with
**748 rendered lookups**, **zero missing** and **zero unused keys**. Retained
proper-name headings count as covered units; tables/ASCII data do not enter
this denominator.

Independent artifact QA against the converter merged in PR #232 found:

- All **1,357 IDs** identical to the baseline; **5,056 internal hrefs** with no
  duplicate IDs, missing destinations or nested anchors.
- **258/258 complete English source runs** and **159/159 prior `<pre>` blocks**
  preserved (417 `<pre>` blocks in the translated output).
- **91/91 tables** unchanged after accounting for translated column labels;
  all ten sections outside Walkthrough unchanged apart from boundary
  previous/next link titles pointing into the translated walkthrough.
- Two independent CLI generations byte-identical. Empty-catalog generation
  left all 623 units in English with explicit fallback notices.
- Hidden T3 browser DOM checks at 1280×800 confirmed 19 chapters, 623/623
  coverage, no missing markers or nested anchors, Portuguese full-text search
  with highlighting, English-title Ctrl+K lookup and readable original text.

The artifact was generated only at
`/mnt/cabal/tmp/dw3-guide-output/guide.html`; the primary checkout's
`docs/dw3-guide.html` remains a post-review/merge owner step. Temporary translator
batches and review reports are under `/mnt/cabal/tmp/dw3-guide-translation/`.
No test suites, game builds, US/EU ISO, emulator or OLED checks were run for
this offline editorial change. Runtime language-pack gates remain open.

## Reading and reference experience (2026-10-24)

The generator now reads its maintained CSS and JavaScript from
`tools/guide_assets/` and embeds both into the output. The result remains one
self-contained offline HTML file, with no external fonts, libraries or images.
Source chapters/areas have a semantic heading hierarchy and labelled sections.
Chapter routes retain **every visit**, including repeated area headings, instead
of deduplicating a return visit. Existing source, chapter and entity IDs are kept.

The field-journal design uses teal navigation and amber landmarks, with quieter
reading surfaces in dark and light themes. The sidebar adds the current
chapter's complete route and reading progress (not game completion). A local
resume link remembers the last chapter on the same browser origin. The route
at the chapter header is a native disclosure; the full text remains readable
without JavaScript. Browser storage failure does not prevent reading.

Search accepts accented or unaccented Portuguese, includes chapter titles, and
can be limited to one guide section. It announces result counts and gives a
clear empty state. The entity catalogue has combined name/source and role
filters, role badges, and collapsed lists of additional definitions/backlinks.
NPC names are indexed only from explicitly named CARD/DIGIMON BATTLE boxes;
quests only from Side Quests chapter titles. This is not an inferred quest log,
and rewards still point to the original recorded encounter/drop data.

Internal navigation reveals hidden/filter-masked targets and their disclosure
ancestors before focusing and scrolling. Context links use `aria-current`;
modal navigation traps focus, restores the trigger on close, and exposes the
active combobox option. The mobile sidebar is inert when closed. Reduced-motion
preferences and no-JavaScript reading are supported. US/EU runtime gates do not
apply to this editorial artifact.

### Reading-experience QA

Local source: the same owner's EmeraldPhoenix page used for #232/#233. Compared
with `c4093b1`: all **1,357 prior IDs retained**; **417/417 decoded pre blocks**,
**91/91 table texts**, and **473/473 translated paragraphs** identical and in the
same order. Coverage stays **623/623**, 748 lookups, zero missing/unused keys.
The output has **469 entities**, **1,389 unique IDs** and **5,317 internal links**;
no missing destinations, duplicate IDs or nested anchors. Existing four ASCII
fallback blocks remain. Two CLI generations were byte-identical.

Nine synthetic regression cases pass with `python3 -B
tools/tests/test_guide_to_html.py -v`; no copyrighted source fixture is committed.
`node --check tools/guide_assets/guide.js` checks the maintained script.

The hidden T3 browser preview was driven with typing, clicks, keyboard navigation
and DOM inspection. Checked dark/light appearances, 1280×800 desktop, 768×1024
tablet, 375×667 mobile and 320×720 narrow layout; the narrow layout also fits at
24px reading text. Verified unaccented Portuguese search/highlighting, section
scope, combined entity filters/empty state, revealing a filtered destination,
context routes, closed-sidebar inertness, combobox active option, modal Tab
containment/Escape/focus restoration. Lookup links jump directly rather than
animating across the entire long document; there is only one sticky-header scroll
offset. Narrow credits and long evolution-tree names wrap instead of widening
the page. Entity name matches rank before matches in source descriptions.

Evidence (local, ignored):
`/mnt/windows/Jogos/dw3-switch/tmp/evidence/2026-10-24-guide-reading/`
contains `generation.log`, `unit-tests.log`, `preservation.json`, desktop
home/chapter/entity screenshots, mobile light screenshot and `browser-qa.md`.
Generated artifact: `/mnt/cabal/tmp/dw3-guide-reading/tmp/guide.html`.
The primary checkout's generated HTML was not replaced. This is editorial QA;
no ISO, emulator, OLED or game build was run. Full screen-reader and cross-browser
certification remain outside this checkpoint. No new prose translation or
inferred NPC/quest/reward facts were introduced.

## Modular offline package (PR #234 follow-up)

**Recommended for reading:** generate a physical chapter package and open its
`index.html`. The single-file command remains compatible, with its historical
CLI default, for exporting one document or printing the whole guide:

```sh
python3 -I tools/guide_to_html.py /path/to/saved-page.txt /mnt/cabal/tmp/dw3-guide --format site
# Open /mnt/cabal/tmp/dw3-guide/index.html in a browser; no server is required.
python3 -I tools/guide_to_html.py /path/to/saved-page.txt /mnt/cabal/tmp/dw3-guide.html --format single
TMPDIR=/mnt/cabal/tmp python3 -B -m unittest discover -s tools/tests -p 'test_guide*.py' -v
```

The user's feedback was structural: an improved table of contents inside a long
page still leaves a monolith. The package therefore puts **each of the 19
walkthrough chapters in its own HTML file**. The overview contains chapter cards
rather than the chapter bodies; orientation notes remain available in a native
disclosure below the index. Reference chapters also have separate pages, while
entities have their own catalogue. The current source produces 58 HTML pages.

A chapter is the reading unit; each area/return visit remains an independently
addressable section on that chapter's page. This preserves story context across
the many short visits, without creating hundreds of tiny pages. The first
chapter has 36 visits. Previous/next links work between physical files, at both
the chapter header and page end; breadcrumbs link back to the chapter overview
and package home. Mobile dock and j/k shortcuts use the same global order.

`tools/guide_site.py` assembles pages from the same rendered fragments used by
the single-file output. It does not retranslate or reparse the English source.
Every existing content ID has a canonical file in a shared route map. Entity
links and source backlinks become relative `file.html#anchor` links, so named
areas, source blocks and techniques remain direct destinations across pages.
A chapter's lightweight section wrapper may repeat its parent's ID locally;
the canonical route still points to that section's overview.

CSS, behavior, global reading-text search index, palette and route map live once
under `assets/`. Pages do not embed the full guide or inline those shared assets.
Classic scripts are intentional: they load from `file://` without fetch,
JavaScript modules, CORS workarounds, a service worker, internet or a web server.
Global search results carry their query to the destination page for highlighting.
Collapsed English-original disclosures are excluded from the shared reading-text
index, as in the compatible single-file search. Native links and all text remain
available without JavaScript; global search/filter/palette require it.

Copy the **whole generated folder**, including `assets/`; open `index.html`.
If a ZIP is supplied, extract it first. Moving just one chapter HTML breaks its
shared assets and cross-page links. `LEIA-ME.txt` travels with the package.
Generation validates all physical files, fragment targets, local assets and
shared metadata routes before writing. Its manifest permits regeneration of
its own files, preserves unrelated files, and rejects conflicting unowned
files and symlinks. The original source and all generated output stay local.

### Package validation checkpoint

The follow-up retains the previous two commits and base on PR #234. With the
owner's unchanged local source, the package has **58 HTML pages / 63 files** and
**1,356 canonical content routes**. All **9,246 local links/resources** resolve.
All **1,389 IDs** in the previous single-file artifact survive in the package
(including shell IDs local to every page). The shared palette script retains
the old `pal-data` ID while loading its data externally.

Comparison by decoded content confirms **417/417 pre blocks**, **91/91 table
texts**, and all **372 source-block texts** identical between the earlier PR
artifact, the compatible single-file output and the package. This includes
both PT-BR and the accompanying English original. Translation coverage stays
**623/623**, 748 lookups, zero missing/unused entries. Both output modes preserve
all 258 complete English walkthrough source runs. Four historical ASCII
fallbacks remain unchanged.

Fourteen synthetic cases cover routing, chapter isolation, backwards-compatible
CLI, cross-page entity/source links, shared search ownership, deterministic
assembly, missing files/fragments/assets, duplicate IDs/nested anchors, safe
regeneration and file conflicts/symlinks. No copyrighted fixture is committed.
The maintained JavaScript passes `node --check`.

The largest walkthrough chapter is approximately **184 KiB**, compared with
approximately **1.58 MiB** for the complete single-file output. The single shared
search/palette/route asset is approximately **520 KiB**. The whole folder is
approximately **3 MiB**: it is larger overall because physical pages repeat
small navigation shells and add the shared search index. This trades modest
local disk space for loading/rendering one chapter at a time; it does not copy
the full guide into every page. The entity catalogue is approximately 403 KiB.

T3's hidden HTTP preview checked the real page changes, chapter overview with
19 cards and no chapter bodies, previous/next across files, entity ↔ source
round trips, global search from chapter 1 into chapter 19 with accent-insensitive
highlighting, palette jumps across files, entity filters and desktop/mobile
layouts. Native T3 preview rejects file URLs, so Playwright CLI with Chrome
headless and a separate temporary profile additionally opened the package by
`file://`, with **browser networking disabled**. Home → overview → chapter 1 →
global search → chapter 19/highlight succeeded without a server; that session
reported no console errors. A JavaScript-disabled offline session also reached
chapter 1 through its native link; DOM inspection confirmed one chapter, no
shared scripts executed, 42 English-original disclosures and a physical next
link. Its post-navigation CLI snapshot failed with `TypeError: Cannot read
properties of undefined (reading 'url')`; this is recorded as a harness
limitation, not a full no-JavaScript browser certification.

Evidence lives under
`/mnt/windows/Jogos/dw3-switch/tmp/evidence/2026-10-24-guide-multipage/`.
The review package is `/mnt/cabal/tmp/dw3-guide-reading/tmp/guide-offline/`,
with a local `guide-offline.zip` alongside it for copying. Extract it and open
`guide-offline/index.html`. No generated guide/ZIP/source was added to git and
the primary checkout's generated HTML remains untouched. ISO/gameplay and
full screen-reader/browser-engine certification remain outside this change.

Section-overview keyboard/dock navigation selects that section's first chapter
(for example, Digimon Evolutions → Angemon), rather than restarting the
walkthrough. Stored preferences/resume are best effort when browser storage is
unavailable; chapter URLs, search and English originals do not depend on it.
