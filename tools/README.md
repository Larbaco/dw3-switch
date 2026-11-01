# Tools

Helpers for extracting and packing game data for the Switch romfs/sdmc layout.

Prefer reusing `vendor/dw3_decomp/tools/extract_disc.py` for the initial disc
unpack; add Switch-specific packers here later.

`l10n/extract_text.py` extracts the game text from the owner's runtime into an editable template
(outside git) and checks a PT-BR translation (`--check`); see
[LOCALIZATION](../docs/LOCALIZATION.md). It never writes inside the repository unless the path
is git-ignored.

`dump_font_cells.py` dumps the font icon cells (PS button icons) from the
owner's runtime for P10.6 ([P10.6_GLYPHS](../docs/P10.6_GLYPHS.md)); output goes
under git-ignored `tmp/` only.

For CPU-only title/options artwork measurements, see
[menu_art_benchmark](menu_art_benchmark/README.md). It needs no game assets,
display or Switch connection and writes results outside the checkout.
