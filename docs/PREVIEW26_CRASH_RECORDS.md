# Preview 26 OLED crash records — 2026-10-14

Scope: record the two crashes reported by the owner while testing
`v0.6.5-preview.26` EU. This is evidence collection only: no diagnosis,
address symbolization, reproduction, or correction was attempted. No roadmap
gate changes follow from this record.

## Collection and provenance

Read-only FTP collection from the owner's Switch succeeded on port `5000`
with anonymous login. Port `21` refused the connection. Only directory
listings and downloads were performed; no SD files were uploaded, deleted,
renamed, or changed. Saves, disc images, extracted assets, and framebuffer
dumps were not downloaded.

Two recent Atmosphere reports were present under
`sdmc:/atmosphere/crash_reports/`. Their associated JPEG files were listed
but not downloaded. Listing timestamps below are exactly as reported by the
FTP server; its timezone was not established and these are not converted to
local time. Both reports name module `dw3-switch` and program ID
`05446530aca7e000`. Attribution to preview 26 is the owner's testing report;
the latest application crash report independently contains that version.

Raw copies are retained locally outside Git at
`/mnt/cabal/tmp/dw3-preview26-crash-evidence/`. The original SD files remain
in place. The local path is a collection artifact, not a portable repository
attachment. No raw logs, screenshots, or game assets are committed here.

## Record 1

- File: `01792006799_05446530aca7e000.log`.
- FTP listing time: `Oct 14 16:39`; size: `15632` bytes.
- SHA-256: `2b69b0452a598cbcb8a47bc8d8503ed6a73bc962802664eae4852019cce52159`.
- Atmosphere report version: `v1.7`.
- Result: `0xCA8 (2168-0006)`; exception type: `User Break`.
- Exception address: `dw3-switch + 0x567980`; break reason: `0x0`.
- Recorded PC: `dw3-switch + 0x56797c`; LR: `dw3-switch + 0x5d44`.
- First return-address offsets: `0x56e7e4`, `0x15c4e8`, `0x7c5fc`,
  `0x7d2ec`, `0x165c54`, `0x165c9c`, `0x165c00`.
- Exact gameplay/menu action preceding this crash was not collected.

## Record 2

- File: `01792007636_05446530aca7e000.log`.
- FTP listing time: `Oct 14 16:53`; size: `15338` bytes.
- SHA-256: `36f98b241eed41d46f5d2311357b4f6735f7815fbb98ca3727ba1d0b588a2244`.
- Atmosphere report version: `v1.7`.
- Result: `0xCA8 (2168-0006)`; exception type: `User Break`.
- Exception address: `dw3-switch + 0x567980`; break reason: `0x0`.
- Recorded PC: `dw3-switch + 0x56797c`; LR: `dw3-switch + 0x5d44`.
- First return-address offsets: `0x56e7e4`, `0x27f70`, `0x165c54`,
  `0x165c9c`, `0x165c00`.
- Exact gameplay/menu action preceding this crash was not collected.

## Latest application report and log

`sdmc:/switch/dw3/crash_report.txt` was listed at `Oct 14 16:53`,
`421` bytes, SHA-256
`2b98c8f6c07d1f47f14311efced6d578446f7eed4118ad93404a5faabca34b3e`.
It contains these literal fields:

```text
version: 0.6.5-preview.26
reason: user exception
mode_file: 0x166
stage_file: 0x1e1
error_desc: 0x101 (Other)
pc:  0x0000000b01b21ad0
lr:  0x0000000b01b21f70
sp:  0x000000006022ad40
fp:  0x000000006022ad40
far: 0x0000000b03386000
esr: 0x92000007
pstate: 0x80000000
x0:  0x0000000b03386000
x1:  0x0000000b03386002
```

Only one `crash_report.txt` was available. Its filename does not identify
separate incidents; there is no preserved per-incident application report
for record 1 in this collection. The two Atmosphere logs provide distinct
records. The listing time of the current application report matches record
2, but that observation alone is not treated as proof of correlation.

`sdmc:/switch/dw3/dw3.log` was also copied: listing time `Oct 14 16:53`,
`552049` bytes, SHA-256
`398623755adb9a73de498af203ad46297652b894b6a6e25a0060a85beb3cc62a`.
Its trailing lines contain repeated `mesh dbg`, `frame dt us`, and
`mem dbg` messages. No cause is inferred from those messages. This collected
log is not assumed to contain both complete crash sessions.

The exception labels, offsets, and application fields above are recorded
as emitted; their meaning and the relationship between the two crashes
remain uninvestigated at the owner's request.
