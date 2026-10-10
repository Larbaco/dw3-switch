# audio_tick_probe (P3.6)

Gdb-only check that `SsSeqCalledTbyT` runs during `dw3-pc` title boot.
Does not modify PSY-Z or the PC CMake link line.

```bash
./tools/audio_tick_probe/probe_ss_seq_called.sh \
  --bin .worktrees/phase3-pc-bringup/build/pc/dw3-pc \
  --runtime .worktrees/phase3-pc-bringup/runtime/us \
  --hits 120
```

Expect `AUDIO_TICK_HITS=120`. Owner `runtime/us` and binaries stay local /
gitignored. See `docs/P3.6_AUDIO.md`.
