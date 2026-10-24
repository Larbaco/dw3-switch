# Tools

Helpers for extracting and packing game data for the Switch romfs/sdmc layout.

Prefer reusing `vendor/dw3_decomp/tools/extract_disc.py` for the initial disc
unpack; add Switch-specific packers here later.

For CPU-only title/options artwork measurements, see
[menu_art_benchmark](menu_art_benchmark/README.md). It needs no game assets,
display or Switch connection and writes results outside the checkout.

`guide_to_html.py` turns a locally saved GameFAQs page of EmeraldPhoenix's
guide into one navigable HTML file (the guide text stays the author's; do not
commit the saved page or the generated HTML):

    python3 -I tools/guide_to_html.py "<saved page>.txt" /mnt/cabal/tmp/dw3-guide.html

Enemies, items, cards, boosters and walkthrough areas are linked only from the
guide's structured cells (Enemies/Items lists, battle boxes, bestiary, booster
lists), never from free prose, into a generated Índice section whose rows link
back to every area where the name appears. Colour variants (`Numemon (Blue)`)
are separate enemies; a Digimon name with more than one chapter links to a
disambiguation row instead of either chapter. The run ends with a link check
(unique ids, no broken internal href or palette entry) and exits non-zero if
it fails.
