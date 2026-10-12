# Preview.20 release checkpoint

Published as a prerelease on 2026-10-12:
[v0.6.5-preview.20](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.20).
Source/tag commit: `ecefe18e11d679a73ec0cf16a7592fffb5d0ed84` (main after #106 and #107).
The official [switch-release run](https://github.com/Larbaco/dw3-switch/actions/runs/38314693895)
succeeded and built both game binaries; this is not the host_hello sample.

Assets and SHA-256:

- `dw3-switch.nro`: 9005601 bytes;
  `8ea2b11d3664776dc79ef140f2fea427f882f11606adef60dfe6fffdea190a98`.
- `dw3-switch-nxlink.nro`: 9026081 bytes;
  `d62a0e50826e2f1d255a0b476ac7d27d7fb301444c2690413b9b7c8c6778f422`.
- `README.txt`: 6152 bytes; installation/version labels were corrected
  using the reviewed documentation from #108 (`52212ac`). The binary
  assets and source tag were unchanged.

Downloaded binaries passed NRO0/ASET header checks. Their NACP display
version is `0.6.5-p.20`; their compiled boot version is
`0.6.5-preview.20`. Both contain the US game runtime path. Only the
nxlink binary contains the `nxlink stdio connected` marker. No game data,
disc images, BIOS, or extracted trees were published.

This cut includes the Start / Port options / Continue shell (#100) and
empty StGetNext scan cache (#106). Menu switches remain stubs without
gameplay effects or persistence. Software battle is the default, GPU
instrumentation from the separate #89 branch is absent, and console
clocks are untouched. Switch menu navigation and OLED movie/performance
validation remain pending at publication; no roadmap hardware gate is
closed by this build.

## Post-release OLED checkpoint (2026-10-12)

The published nxlink binary was transferred and launched through hbmenu
NetLoader. The session connected nxlink stdio (`fd=3`, socket init success),
initialized EGL at 1280x720 handheld and audren at 44100 Hz stereo, and
reported pad kind `0x73`. The physical controller model was not recorded.

Owner-confirmed title test: Plus → D-pad Port options → B opens the shell
with both options OFF; X closes back to the middle title row. Confirmation:
“Sim, aparece OFF e volta corretamente”. P9.2 is complete with the earlier
PC TTB checkpoint; the stub effects/persistence and Phase 9 gate remain open.

STR/MDEC frame-ready counters progressed through 1–30 and later 60–120;
XA sector decoding was active. Start skipped back to the title twice.
These are execution observations, not movie quality or synchronization
passes. Asked about the opening video, the owner reported:
“Ainda tem erros ou lentidão”. The report does not identify whether image,
audio, pacing or multiple areas remain affected. Movie quality remains open;
a regression and a quantified performance gain from #106 are not established.

Applet exit logged `flat saves flushed`; the nxlink process exited with code 0.
No fresh-save/relaunch reload test was performed, so P5.3 remains open.
Local evidence: `/tmp/dw3-preview20-test/nxlink.log` (532 lines); raw logs
and owner game imagery are not repository assets. Publication metadata
and binary hashes above remain unchanged.
