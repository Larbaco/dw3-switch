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
platforms/switch/   Switch host only (libnx, present, audio, input, FS)
port/               Platform-independent glue (CD→FS, overlays, memcard)
tools/              Extract/pack helpers for Switch layout
docs/               Design notes for THIS port
vendor/dw3_decomp/  Upstream submodule — reference + game C source
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

1. **Shim, don’t reimplement the console.** Prefer Psy-Q compatible layers
   (PsyCross / PSY-Z style) over a from-scratch GPU/GTE/SPU stack.
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

This repo is followed from more than one machine. Keep the **remote** as the
shared timeline — local-only progress is not enough.

- **Commit frequently** after each small, reviewable unit of work (scaffold,
  fix, doc note, stub). Prefer several small commits over one large dump.
- **Push frequently** after those commits: `git push -u origin HEAD` on the
  first push of a branch, then `git push`. Do not leave finished work only on
  the local disk between sessions.
- Work on a named feature/topic branch when the change is more than a tiny
  doc tweak; keep that branch on the remote so another PC can `git fetch` /
  `git pull` and continue.
- At session start on any machine: `git fetch` / `git pull` (or rebase onto
  the tracking branch) before editing, so you do not diverge from what was
  pushed elsewhere.
- Do not force-push, or rewrite history unless asked.
- Do not change git config.
- Stage files explicitly; do not `git add .` / `git add -A`.
- Do not commit secrets, keys, disc images, or extracted game trees.
- Submodule updates (`vendor/dw3_decomp`) must be intentional and called out
  in the commit message.
- For this project, frequent commit + push is the default (no need to wait for
  an explicit “commit/push” prompt each time), unless the owner says to hold.

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
2. `git fetch` / sync with the remote tracking branch; check `git status` and
   submodule state.
3. Continue the current roadmap phase; do not restart scaffolding.
4. Before ending a session (or when leaving a coherent checkpoint), commit and
   push so another PC can pick up from the remote.
