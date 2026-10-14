# Preview.26 release checkpoint

Published 2026-10-14: [v0.6.5-preview.26](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.26).
Exact source: `d3fc498125d360c76a05b86db3c394f06b6e202b`.
Full boot version: `0.6.5-preview.26`; NACP: `0.6.5-p.26`.
This documentation follow-up does not change the tagged build source.

## Published bundle and provenance

The seven assets are US ordinary/nxlink NROs, EU ordinary/nxlink NROs,
`README.txt`, `SOURCE.txt`, and `SHA256SUMS`. No game assets are included.
The US pair comes from [release run 38623237485](https://github.com/Larbaco/dw3-switch/actions/runs/38623237485).
The EU pair uses `tools/build_switch_game.sh --eu` (and `--nxlink`),
with `RELEASE_TAG=v0.6.5-preview.26` and the same exact source.
`SOURCE.txt` records Docker image `devkitpro/devkita64:latest` as
`sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`.
The packaged README adjusts candidate wording to published release notes.

Published NRO SHA-256 values:

```text
6680a52ed84660f4bc9c700584f6c0f8b9b749d3569b0acf89f51290fbd00672  dw3-switch.nro
aba8f4e5aedfa6963ffdf27f48ee98a31dfdb4094174c52621ff0f6a429d345d  dw3-switch-nxlink.nro
29b78a5486e97284e7a87f8a86db3408d7c854b7c157233fe5771c913b9b4486  dw3-switch-eu.nro
e5f82b644650d6ef17d7dc6ee6d9283d5082ebcd308c5359a27a65cbe7d03ab9  dw3-switch-eu-nxlink.nro
```

These match the published GitHub asset digests and downloaded `SHA256SUMS`.
This documentation audit did not independently download or execute the NROs.

## Changes and OLED test gates

- #143: original EU language selection before movie/title; US boot unchanged.
- #148: idle-camera transforms, opaque black portrait pixels, and blend-state
  restoration for HUD/title after battle fades.
- #149: revised Port options screen with contextual help.
- #152: restart-help correction following #151; restart returns to boot flow.
- #146 field-turbo raster skipping is excluded. Existing R3 turbo remains.

Publication and successful builds do not establish hardware acceptance.
At this checkpoint no preview.26 OLED delivery or gameplay is claimed here.
Validate options navigation and values, EU language → movie → title → Continue,
Agumon eyes and battle idle camera, HUD after battle, and battle → normal save
→ restart → Continue. Check turbo and encounter transitions for regressions;
a displayed frame rate alone does not establish the gameplay multiplier.
Known battle sky gaps, the P5.2 performance gate, and option persistence remain open.
Settings stay session-only and restart stays opt-in. Preserve both region trees
and their saves; extraction tests use the NRO and the owner's matching BIN/CUE.
See [HOTKEYS](HOTKEYS.md) and [PORT_OPTIONS](PORT_OPTIONS.md).
