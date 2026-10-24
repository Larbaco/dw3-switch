# Local GameFAQs guide HTML

`tools/guide_to_html.py` converts the locally saved GameFAQs HTML page (even
when saved with a `.txt` extension) into a standalone, offline HTML guide.
Python 3 and its standard library are sufficient:

```sh
python3 -I tools/guide_to_html.py /path/to/saved-page.txt /mnt/cabal/tmp/guide.html
python3 -B tools/tests/test_guide_to_html.py -v
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
through Ctrl+K and the sidebar's entity index.

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
Five synthetic regression tests cover ambiguity, collisions, markup/escaping,
boundaries, ASCII preservation, backlinks and integrity failures.

A hidden T3 browser tab at 1280x800 also checked the live DOM and followed
walkthrough → Renamon's Digimon/card entry → walkthrough. No OLED, emulator,
ISO or gameplay validation was performed: this utility does not load either
US or EU game data and does not change the port runtime.
