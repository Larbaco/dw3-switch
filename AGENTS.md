# AGENTS.md — dw3-switch

Read this file before changing anything in `/mnt/windows/Jogos/dw3-switch`.
It is the AI source of truth for this folder. Prefer it over chat history when
they conflict.

Owner context: Thiago (Backend / DevOps). Prefer concise Portuguese in chat;
keep code, commits, and technical identifiers in English.

## Mission

Build a **Nintendo Switch homebrew native port** of Digimon World 3 / Digimon
World 2003, using the matching PS1 decompilation as the game-logic source.

This is **not** an emulator wrapper project and **not** a remake from scratch.

## Source of truth

| Doc | Use for |
|---|---|
| `README.md` | Project overview and layout |
| `docs/ROADMAP.md` | Phased goals; do not skip ahead without reason |
| `docs/ARCHITECTURE.md` | Host / shim / game layering |
| `vendor/dw3_decomp/README.md` | Upstream decomp build and status |
| `vendor/dw3_decomp/docs/` | Binaries, overlays, toolchain facts |

If docs disagree with code, trust code and update the docs in the same change.

## Repository layout (boundaries)

```
platforms/pc/       PC host (PSY-Z / SDL3 bring-up)
platforms/switch/   Switch host only (libnx, present, audio, input, FS)
port/               Platform-independent glue (CD→FS, overlays, memcard)
tools/              Extract/pack helpers for Switch/PC runtime layout
docs/               Design notes for THIS port
vendor/dw3_decomp/  Upstream decomp (git submodule)
vendor/psyz/        PSY-Z Psy-Q shim (git submodule, Phase 1+)
```

Rules:

- Put Switch-specific code under `platforms/switch/`.
- Put reusable glue under `port/`, not inside the Switch tree.
- Do **not** vend game assets into git. `disks/`, `romfs/game/`, `*.bin`,
  `*.cue`, `*.iso` are gitignored on purpose.
- Treat `vendor/dw3_decomp/` as upstream. Prefer patches/glue in `port/` or
  `platforms/`. Only edit the submodule when there is a clear upstream-bound
  fix, and say so explicitly.
- Do not rewrite the decomp into a new engine “for cleanliness” unless the
  owner asks for that scope change.

## Technical directives

1. **Shim, don’t reimplement the console.** Prefer **PSY-Z** (Psy-Q compatible)
   over a from-scratch GPU/GTE/SPU stack; PsyCross-style layers only when PSY-Z
   does not cover a needed API.
2. **PC bring-up before Switch polish** when a change is easier to validate on
   desktop; keep the Switch host thin.
3. **No matching-build cargo on Switch.** Byte-identical PS1 output is the
   decomp’s job. The port may use a modern compiler and relocatable modules.
4. **Overlays/stages need an explicit loader story.** Do not assume fixed PS1
   VRAM addresses work as-is on AArch64.
5. **Own-disc only.** Never add, commit, or upload original game files, BIOS,
   or redistributable ROMs.
6. **Keep Phase 0/1 honest.** A minimal `.nro` (boot, framebuffer, pad, log)
   comes before linking game code.

## Git and safety

- Commit frequently
- Do not force-push, or rewrite history unless asked.
- Do not change git config.
- Stage files explicitly; do not `git add .` / `git add -A`.
- Do not commit secrets, keys, disc images, or extracted game trees.
- Submodule updates (`vendor/dw3_decomp`) must be intentional and called out
  in the commit message.

## Working style

- Small, reviewable changes aligned to the current roadmap phase.
- Prefer extending existing scaffold paths over inventing parallel trees.
- When blocked on missing hardware/SDK (devkitPro, real Switch), say what is
  missing and leave the tree in a buildable or clearly stubbed state.
- Record non-obvious decisions in `docs/` (short notes), not only in chat.

## Out of scope (unless explicitly requested)

- Shipping or hosting game assets
- Matching decompilation work inside `vendor/dw3_decomp` as the main task
- Official Nintendo SDK / eShop packaging
- Cheats, piracy tooling, or bypassing console security beyond normal
  homebrew (Atmosphere + homebrew `.nro`) assumptions

## First actions for a new session

1. Read this file, `README.md`, and `docs/ROADMAP.md`.
2. Check `git status` and submodule state.
3. Continue the current roadmap phase; do not restart scaffolding.
