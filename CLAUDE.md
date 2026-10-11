# CLAUDE.md — dw3-switch

Read **`AGENTS.md` first**. It is the AI source of truth for this repo; prefer
it over chat history when they conflict.

## Mission

Nintendo Switch homebrew **native port** of Digimon World 3 / Digimon World
2003, driven by the matching PS1 decompilation. Not an emulator wrapper and
not a from-scratch remake.

## Orchestration

- Parent chat agent = **orchestrator**: plans, reviews, merges PRs, updates
  the owner. Does not investigate/test/code when a subagent can.
- **Subagents** own investigation, measurement, implementation, local
  builds/tests, commits, and opening PRs.
- Each workstream: own **branch** + own **worktree** (prefer
  `/tmp/dw3-<topic>` on ext4).
- Commit and push frequently; open a PR against `main` when done.
  Orchestrator reviews and merges.

## Pointers

- Phased goals: `docs/ROADMAP.md`
- Overview and layout: `README.md`
- Full rules, layout boundaries, and Switch/nxlink notes: `AGENTS.md`

## Owner prefs and hard limits

- Concise Portuguese in chat with the owner; English in code and commits.
- Own-disc only. Never commit game assets, BIOS, or redistributable ROMs.
- Do not overclock the Switch.
