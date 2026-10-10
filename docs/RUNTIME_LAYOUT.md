# Runtime data layout (PC + Switch)

Shared relative tree for Digimon World 3 disc data after extract + pack.
PC and Switch hosts use the same layout; only the root path differs.

Game bytes stay out of git (`/disks/`, `/runtime/` are gitignored).

## Paths

| Host | Root |
|---|---|
| PC (dev) | `runtime/us/` (or `runtime/eu/`) under the repo |
| Switch SD | `sdmc:/switch/dw3/us/` (or `.../eu/`) |

Phase 1 documents the Switch path only; the `.nro` does not load this tree yet.

## Tree (US)

```text
runtime/us/                 # or sdmc:/switch/dw3/us/
  files_us.json             # file-id → ISO path / LBA map (from build_file_map.py)
  SLUS_014.36               # executable
  BOOT_IMAGE.txt            # Phase 1 stub (see boot image below)
  # boot_tim.bin            # Phase 2+: exe-tail TIM when extract is available
  AAA/                      # disc AAA tree (PRO overlays, assets, …)
    PRO/
      *.PRO
    …
```

EU uses `files_eu.json` and `SLES_039.36` with the same `AAA/` shape.

## How to produce

Prerequisites: owner extract at `disks/<ver>/` including `files_<ver>.json`
(see `docs/DISC_SETUP.md` / Task 7).

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
python3 tools/build_file_map.py --check runtime/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

`pack_runtime.py` refuses to run if `files_<ver>.json` is missing.

## Boot image

On disc, the boot TIM is the executable tail loaded at `OVERLAY_VRAM`
(`0x80082448` US; see `docs/SURVEY.md` and decomp `main()` / `BOOT_IMAGE_RECT`).

Phase 1 does **not** invent `boot_tim.bin`. If `disks/<ver>/boot_tim.bin` is
absent, the packer writes `BOOT_IMAGE.txt` explaining the Phase 2 extract.
Hosts that need a boot framebuffer should stub or skip that `LoadImage` until
then.

## Relationship to `disks/`

| Dir | Role |
|---|---|
| `disks/<ver>/` | Raw extract + map (source of truth for packing) |
| `runtime/<ver>/` | Host-facing subset: map, exe, `AAA/`, boot stub/bin |

Do not commit either tree.
