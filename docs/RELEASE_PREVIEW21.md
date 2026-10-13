# Preview.21 release checkpoint

Published as a prerelease on 2026-10-13:
[v0.6.5-preview.21](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.21).
Source/tag commit: `b597c67a454f3f0d73c2c70cbb8f49e9faaa52b9` (main after
#111 and #112). Built by the official
[switch-release run](https://github.com/Larbaco/dw3-switch/actions/runs/38344928149)
(`workflow_dispatch`); this is not the host_hello sample.

Assets and SHA-256:

- `dw3-switch.nro`: 9005601 bytes;
  `d575432a7376519e79cb92e9ab3a8abdfaedb7d66af5db3423104dd912e54b1b`.
- `dw3-switch-nxlink.nro`: 9026081 bytes;
  `389551fe668cd4837a963ca70354706be3e23d6eea41d36c09da9af9a4d32f24`.

The byte sizes match preview.20, but the contents and hashes differ.
Boot version is `0.6.5-preview.21`; NACP display version is `0.6.5-p.21`.
Only the nxlink binary contains the `nxlink stdio connected` marker.
No game data, disc images, BIOS, or extracted trees were published.

The release body and the `README.txt` asset were copied from preview.20 at
publication time. The corrected `platforms/switch/release/README.txt` in
the repository is the source for the asset update.

## Contents

- STR file-bridge read-race fix (#111): the opening movie's video and XA
  readers now share `Psyz_AudioLock` around the file handle.
- Empty `StGetNext` scan cache (#106) and the title Port options shell
  (#100) remain. The shell switches are stubs with no gameplay effects or
  persistence.
- Software battle is the default. GPU diagnostics from #89 are absent.
  Console clocks are untouched.

## OLED validation (2026-10-13, nxlink build)

Opening movie `MOVIEOPN.STR`:

- 14.994-15.006 fps over 18 steady 60-pump windows (about 4.00 s each).
  preview.20 measured 6.75 fps.
- All 20 `movie dbg:` lines show `fc_skipped=0 restarts=0`.
- `fc` advances 1:1 with delivered frames (1..1110). It was about 2.3 `fc`
  per frame before the fix.
- No ring backpressure or wrap-wait lines.
- The owner confirmed the XA audio sounds correct.

Limits:

- The owner pressed Start at `fc=1110` of about 1776. The last ~38% of the
  movie was not played on hardware. Ryubing played the whole movie with
  zero drops.
- Image quality was not separately assessed.
- Fresh-save reload (P5.3) and the other preview.20 items remain as they
  were; this release does not close them.

Details: [movie pacing diagnostics](P3.7_MOVIE_PACING_DIAGNOSTICS.md).
