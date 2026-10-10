# Upscaling research (FSR / DLSS)

Phase 11 goal note. See [ROADMAP.md](ROADMAP.md) P11.1–P11.4.

This is an **investigation track**, not a promise to ship temporal upscaling.
The playable port must not wait on FSR/DLSS. Integer scale, aspect fit, and
optional widescreen (P6.1) remain the supported present path until a spike
says otherwise.

## Why it is hard here

| Constraint | Implication |
|---|---|
| Game renders through PSY-Z (PS1 GPU emulation) | Need a stable color (and ideally depth/motion) buffer at present time |
| DW3 mixes 2D pre-rendered fields and 3D battles | Temporal upscalers without good vectors often shimmer or ghost UI |
| Nintendo Switch homebrew | **DLSS is NVIDIA/PC-only** — never on Switch. FSR-class spatial filters may be possible on EGL/deko3d if we own the present pass |
| No matching-build / eShop SDK goal | Cannot depend on retail upscaler APIs |

## Candidates (rough order of realism)

1. **Spatial upscale / FSR 1–style** on the host present pass (PC first) — most plausible.
2. **Sharper bilinear / Lanczos / CAS** — cheap baseline for comparison in the same spike.
3. **FSR 2/3 or other temporal** — only if PSY-Z (or our present path) can supply motion vectors / depth reliably; likely **no-go** initially.
4. **DLSS** — PC + NVIDIA only; treat as optional experiment if a Vulkan/DX12 present ever exists; default **out of scope** for Switch and for the SDL3_GPU path unless the spike finds a clean hook.

## Layering

| Layer | Role |
|---|---|
| `platforms/pc` (spike) | Prototype present-time upscale; capture before/after |
| `platforms/switch` | Follow-up only if PC go + libnx GPU path allows |
| `port/` / Phase 9 | Toggle only if something ships; default off |
| This doc | Record go / no-go / blockers after P11.2 |

## Relation to other phases

- **P6.1** — production scaling/widescreen; not blocked by Phase 11.
- **P8** — texture packs replace source art; orthogonal to present upscaling.
- **Phase 10** (native Switch residual, when present on a branch) — lifecycle/rumble/etc.; upscaling research stays Phase 11.

## Out of scope

- Claiming “DLSS on Switch”
- Blocking Phase 3–5 gates on upscaler R&D
- Vendoring large proprietary SDKs without an explicit owner decision after the spike
