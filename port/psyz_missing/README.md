# port/psyz_missing

Minimal Psy-Q stubs for APIs declared by PSY-Z headers but missing or
incomplete in the pinned PSY-Z build (title bring-up). Prefer fixing
upstream PSY-Z later; keep this thin.

`psyz_pad_host.c` is the host libpad implementation (`PadInitDirect` →
`InitPAD`, `PadGetState` → connected when PSY-Z reports a pad frame). It is
compiled into `dw3-pc` as a direct TU so it overrides PSY-Z's
`PadGetState` stub. Headless check: `pytest tools/tests/test_pad_host.py`.
