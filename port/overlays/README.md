# Host overlays (Phase 2)

## E3 / P2.3 — address symbols

`host_vram.c` provides `OVERLAY_VRAM`, `STAGE_VRAM`, `OVERLAY_ADDRESS`, and
`SUB_OVERLAY_ADDRESS` for the host engine build. Sizes are padded US regions
(256 KiB each).

## E5 / P2.6 — static overlay model

On PS1, `loadModeOverlay` / `loadSubOverlay` `memcpy` a `.PRO` into fixed
VRAM **only when the tracked id changes**. libnx has no `dlopen`, and the
port links game C into one image, so “load” becomes:

| Event | Host action |
|---|---|
| Mode / sub file id **changes** | Restore that overlay’s initial `.data`, **zero** its `.bss` |
| Same id again | No-op — overlay globals **preserved** |

`overlay_host.c` implements `dw3_overlay_restore`. Patch
`0008-PORT_HOST-static-overlay-restore.patch` wires the decomp loaders to
it. Descriptor table: `{file_id → entry, data, data_init, data_size, bss,
bss_size}`.

Host zeros `.bss` on id change even though the PS1 CD image does not clear
it after a `memcpy`; that keeps the static model deterministic.

### Failed restore (patch 0061)

The loaders call `dw3_overlay_load_mode` / `dw3_overlay_load_sub`, which
hold the rule above and handle a file id that is not in the host table (an
overlay that is not linked, such as `WFIGHTTS`, `SOUNDTST`, `SHOCKTST`, or
the skipped `WSTAG460`):

| What | Behaviour |
|---|---|
| Log | the stable `dw3: overlay restore file=0x… kind=unknown UNKNOWN` marker, then `dw3: ERROR overlay restore failed: file=0x… (<dec>): <reason>` |
| Latch | `OVERLAY_LOADER.mode` / `.subOverlay` set to `-1`, not to the id, so the next load (same id or the previous one) restores again |
| Strict mode | `abort()` after a `dw3: FATAL …` line. On when `DW3_REPLAY` is set (every P6.15 replay run) or `DW3_OVERLAY_STRICT=1`; `DW3_OVERLAY_STRICT=0` turns it off; the Switch replay build (`DW3_NX_REPLAY`) calls `dw3_overlay_set_strict(1)` |
| Return | `-1` (`0` restored, `1` same id / no-op) |

On the host every overlay's code and data are linked into the image, so a
failed restore does not run stale bytes: `STAGE_VRAM` /
`SUB_OVERLAY_ADDRESS` is only read once, for the boot TIM
(`src/engine/system/main.c`), never as overlay code or data. What a failed
restore does mean is that the overlay's globals were not reset (if it is
linked but not registered) or that its entry is a weak stub returning NULL
(`mode_stubs.c`, if it is not linked). Before 0061 that was a single log
line and the id was recorded as loaded.

### Same sub-overlay twice (battles)

`FIGHTSTG_updateRoot` loads `WFIGHTMN` (or `WFIGHTTS`) with
`loadSubOverlay` at the start of every battle; the same-id skip keeps its
globals only while the mode does not change. On the PS1 that is the same
rule: `loadSubOverlay` skips the `memcpy` for the same id, and
`loadModeOverlay` sets `subOverlay = -1` on every mode change. Every way out
of a battle is a mode change (`WFIGHTMN_endBattle`: field mode on flee,
`MODE_BATTLE_REPORT` (`STFGTREP`) or `MODE_ENDING` on a win, `MODE_TITLE` on
a loss), and nothing requests `MODE_BATTLE` from inside a battle, so the
next battle always reloads `WFIGHTMN` on the PS1 and restores its `.data` /
`.bss` on the host. The P6.15 scenarios `us-two-battles` /
`eu-two-battles` check this (two battles with a technique each; the log
shows `FIGHTSTG` + `WFIGHTMN` restored for each).

`WFIGHTMN_bringLastEnemy`'s `while (1)` over `WFIGHTMN_elementVisuals`
(`menu.c`, final battle only) has no bound check, but the table covers every
element (2..8, plus a `-1` row the loop does not test) and is never
written, so the host's restored copy is the same as the linked one; it can
only run off the table for an element the PS1 would also run off for.

### Symbol-prefix strategy (P3.2)

Phase 2 kept only `STDWTITL` linked. P3.2 uniquifies **stage** globals with
`tools/prefix_stage_obj.py` (`objcopy --redefine-syms`) and packs mode
overlays with `tools/rename_ovl_sections.py` (`ld -r` + section rename /
`merge_ovl_sections.ld`). Writable sections become `dw3data_<name>` /
`dw3bss_<name>`; GNU ld supplies `__start_`/`__stop_` orphans.
`overlay_sections.c` binds those into `Dw3OverlayDesc` snapshots.

Unlinked stages get weak stubs from `tools/gen_stage_stubs.py`.
`mode_stubs.c` must **not** weak-stub overlays that are really linked
(archive pull / strong defs).

### US overlay table (title + field path)

| Overlay | File id (US) | Role |
|---|---|---|
| `STDWTITL` | `0x1C1` | Title |
| `FIELDSTG` | `0x158` | Field mode |
| `STPLNMET` | `0x1F8` | Player name (new game) |
| `WSTAG260` | `462` / `0x1CE` | Story scripts (data) |
| `WSTAG780` | `474` / `0x1DA` | Kusanagi street (new-game start) |
| `WSTAG785` | `475` / `0x1DB` | Online Center (door target) |
| `WSTAG790` | `476` / `0x1DC` | Chamber Room (post-name) |
| `WSTAG200`…`895` + Amaterasu twins (batch1–6; skip `WSTAG460`) | see `docs/P3.8_STAGES.md` | All US stages except known-bad `WSTAG460` |
| `FIGHTSTG` | `0x159` | Battle mode (P3.3) |
| `WFIGHTMN` | `0x1FA` | Normal battle sub-overlay (P3.3) |
| `STFGTREP` | `0x1C2` | Battle report / XP apply (P3.3) |
| `STSTATUS` | `0x1F9` | Field menu screens (P3.4) |
| `STITSHOP` | `0x1F7` | Item shop (P3.4) |
| `STGTRAIN` | `0x1F6` | Training gym (P3.4) |
| `STGDGLAB` | `0x1DF` | Digimon Lab (P3.4) |
| `STGMCARD` | `0x1E3` | Memory card (P3.4) |
| `STDGNAME` | `0x154` | Partner nickname (P3.4) |
| `CARDGAME` | `0x153` | Card battle (P3.5) |
| `STCRDDEK` | `0x1BC` | Deck editor (P3.5) |
| `STCRDSHP` | `0x1C0` | Card pack shop (P3.5) |
| `STCRDABM` | `0x177` | Card album (P3.5) |
| `STAGSLCT` | `0x176` | Debug stage select (P3.8) |
| `CNTY_SEL` | `0x157` (EU `0x165`) | Country / language select (P6.2) |

`overlay_table_us.c` registers the rows above for US; `overlay_table_eu.c`
is selected with `-DDW3_VERSION=eu` (see `docs/P3_EU_BUILD.md`). `CNTY_SEL`
is linked in both (`dw3_p62_cnty_sel`); the EU build boots into it (patch
`0055`) and it sets `LANGUAGE`. On US it is reachable only from STAGSLCT.
Host objects use named `.data`/`.bss` sections (`tools/rename_ovl_sections.py`,
`tools/merge_ovl_sections.ld`) and snapshots in `overlay_sections.c`. Stage TUs
are symbol-prefixed (`tools/prefix_stage_obj.py`) and linked via `dw3_p32_field`;
battle via `dw3_p33_battle`; menus via `dw3_p34_menus`; cardgame via
`dw3_p35_cardgame`; stage select via `dw3_p38_stagslct`; country select via `dw3_p62_cnty_sel`. Runtime notes:
`docs/P3.2_FIELD.md`, `docs/P3.3_BATTLE.md`, `docs/P3.4_MENUS.md`,
`docs/P3.5_CARDGAME.md`, `docs/P3.8_STAGES.md`.

### Host test

```bash
# from repo root
python3 -m pytest tools/tests/test_overlay_host.py -v

# or directly:
mkdir -p build
gcc -std=c11 -Wall -Wextra -Werror -O2 -DPORT_HOST=1 \
  -o build/overlay_host_test tools/overlay_host_test.c \
  port/overlays/overlay_host.c -Iport/overlays
./build/overlay_host_test
```

Fixtures prove: two different overlays reset; the same overlay twice
preserves mutated `.data` / `.bss`; an unknown id fails, is not recorded as
loaded (the next load restores again), and aborts in strict mode. The test
drives the real `dw3_overlay_load_mode` / `dw3_overlay_load_sub`.

### Switch / P4.6

libnx has no `dlopen`; the Switch image uses this same static restore path
(no `Psyz_ModuleOpen`). Hardware acceptance is an OLED nxlink restore
sweep — checklist and log parser:

- `docs/P4.6_MODULES_STATUS.md`
- `python3 tools/parse_overlay_nxlink.py <nxlink-capture>`

Restore lines include `kind=mode` (entry != NULL) or `kind=sub`; a failed
restore prints `kind=unknown UNKNOWN` plus a `dw3: ERROR overlay restore
failed` line (and aborts in a replay build).

## Boot TIM

On PS1 the boot TIM is the packed exe tail at `OVERLAY_VRAM`, read via
`SUB_OVERLAY_ADDRESS`. Host `main` stubs that upload until
`runtime/us/boot_tim.bin` is loaded (see `docs/RUNTIME_LAYOUT.md`; wire in E6).
