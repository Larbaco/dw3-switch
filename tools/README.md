# Tools

Helpers for extracting and packing game data for the Switch romfs/sdmc layout.

Prefer reusing `vendor/dw3_decomp/tools/extract_disc.py` for the initial disc
unpack; add Switch-specific packers here later.

For CPU-only title/options artwork measurements, see
[menu_art_benchmark](menu_art_benchmark/README.md). It needs no game assets,
display or Switch connection and writes results outside the checkout.
