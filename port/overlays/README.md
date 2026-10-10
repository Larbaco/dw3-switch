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
| `FIGHTSTG` | `0x159` | Battle mode (P3.3) |
| `WFIGHTMN` | `0x1FA` | Normal battle sub-overlay (P3.3) |

`overlay_table_us.c` registers the rows above; `CNTY_SEL` stays out for US.
Host objects use named `.data`/`.bss` sections (`tools/rename_ovl_sections.py`,
`tools/merge_ovl_sections.ld`) and snapshots in `overlay_sections.c`. Stage TUs
are symbol-prefixed (`tools/prefix_stage_obj.py`) and linked via `dw3_p32_field`;
battle via `dw3_p33_battle`. Runtime notes: `docs/P3.2_FIELD.md`,
`docs/P3.3_BATTLE.md`.

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
preserves mutated `.data` / `.bss`.

## Boot TIM

On PS1 the boot TIM is the packed exe tail at `OVERLAY_VRAM`, read via
`SUB_OVERLAY_ADDRESS`. Host `main` stubs that upload until
`runtime/us/boot_tim.bin` is loaded (see `docs/RUNTIME_LAYOUT.md`; wire in E6).
