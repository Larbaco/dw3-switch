# Phase 1 — Toolchains, Hosts, Data Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproducible Docker/Switch and PC hosts, PSY-Z vendored, and a US disc → runtime layout pipeline — with no game logic linked yet.

**Architecture:** PC bring-up host (`platforms/pc`) links PSY-Z (SDL3 backend). Switch host (`platforms/switch`) is a thin libnx `.nro` built via `tools/dk.sh` + `devkitpro/devkita64`. Disc files stay out of git; `tools/build_file_map.py` and `tools/pack_runtime.py` turn the owner's US image into an on-disk runtime tree keyed by numeric file id.

**Tech Stack:** Docker (`devkitpro/devkita64`), libnx/devkitA64, CMake ≥ 3.21, PSY-Z (`vendor/psyz` + its `external/SDL`), Python 3.12, GitHub Actions.

## Global Constraints

- Work on branch `phase1-toolchains-hosts` (never commit Phase 1 work to `main` until the gate passes and the owner asks to merge).
- Primary game version for this cycle: **US** (`VERSION=us`, serial `SLUS-01436`).
- Owner disc (do not copy into git): `/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin` (SHA-1 `f0b022f9be53cbce14640abd8f01beaadcb35208`).
- Never commit `disks/`, `romfs/`, `*.bin`, `*.cue`, `*.iso`, extracted `AAA/`, or any original game bytes.
- Prefer Portuguese only in chat with the owner; code, paths, commits, identifiers in English.
- Stage files explicitly (`git add path…`); no `git add -A` / `git add .`.
- Submodule adds/bumps must be called out in the commit message.
- Keep `platforms/switch/` free of game logic; PC host is stub-only in this phase.
- PSY-Z PC renderer default: `-DPSYZ_RENDERER=sdl3_gpu` (PSY-Z Linux default).
- Pin PSY-Z at commit `fac9bd58ccd8a0c35e31c6354528f9d625e9bdd1` unless that commit fails to build; then pin the nearest later green commit and document it in `docs/PSYZ.md`.
- Host needs CMake: install `cmake` + build deps via apt if missing (`sudo apt-get install -y cmake ninja-build pkg-config libgl1-mesa-dev`).
- Follow `AGENTS.md`, `docs/ROADMAP.md` Phase 1, `docs/SURVEY.md`, `docs/ARCHITECTURE.md`.

## File map (create / modify)

| Path | Role |
|---|---|
| `tools/dk.sh` | Run commands in `devkitpro/devkita64` with repo mounted |
| `tools/docker/README.md` | How the two Docker images are used |
| `vendor/psyz/` | PSY-Z git submodule (+ recursive SDL) |
| `docs/PSYZ.md` | Pin, license, renderer choice |
| `platforms/pc/CMakeLists.txt` | PC host CMake |
| `platforms/pc/src/main.c` | Stub: clear screen, quit on window close / Start |
| `platforms/pc/README.md` | Build/run instructions |
| `platforms/switch/Makefile` | libnx homebrew Makefile |
| `platforms/switch/source/main.c` | Stub `.nro`: console, pad, nxlink-friendly logs |
| `platforms/switch/README.md` | Build via `tools/dk.sh` |
| `tools/build_file_map.py` | Disc + `FILE_SECTORS` → `files_us.json` |
| `tools/pack_runtime.py` | `disks/us` → runtime tree |
| `tools/testdata/mini_iso.bin` | Tiny synthetic MODE2/2352 image for unit tests |
| `tools/testdata/file_sectors_us_snippet.c` | Tiny FILE_SECTORS fixture for unit tests |
| `tools/tests/test_build_file_map.py` | Unit tests (no real disc) |
| `docs/DISC_SETUP.md` | Owner extract steps for US disc |
| `docs/RUNTIME_LAYOUT.md` | PC + `sdmc:/switch/dw3/us/` layout |
| `.github/workflows/ci.yml` | Build PC stub + Switch `.nro` in Docker |
| `AGENTS.md` | Add `platforms/pc/`, `vendor/psyz` |
| `.gitmodules` | `vendor/psyz` entry |
| `.gitignore` | `build/`, runtime dirs, ensure discs stay ignored |
| `docs/ROADMAP.md` | Mark completed Phase 1 items when done |
| `README.md` | Point at new hosts / disc setup |

---

### Task 1: Feature branch and layout docs

**Files:**
- Modify: `AGENTS.md`
- Modify: `README.md`
- Create: (none required beyond branch)

**Interfaces:**
- Consumes: none
- Produces: branch `phase1-toolchains-hosts`; `AGENTS.md` lists `platforms/pc/` and `vendor/psyz`

- [ ] **Step 1: Create the branch from current `main`**

```bash
cd /mnt/windows/Jogos/dw3-switch
git checkout main
git pull --ff-only origin main || true
git checkout -b phase1-toolchains-hosts
```

Expected: `git branch --show-current` prints `phase1-toolchains-hosts`.

- [ ] **Step 2: Update layout in `AGENTS.md`**

Replace the layout block with:

```
platforms/pc/       PC host (PSY-Z / SDL3 bring-up)
platforms/switch/   Switch host only (libnx, present, audio, input, FS)
port/               Platform-independent glue (CD→FS, overlays, memcard)
tools/              Extract/pack helpers for Switch/PC runtime layout
docs/               Design notes for THIS port
vendor/dw3_decomp/  Upstream decomp (git submodule)
vendor/psyz/        PSY-Z Psy-Q shim (git submodule, Phase 1+)
```

Also update technical directive 1 to name PSY-Z explicitly (not PsyCross as primary).

- [ ] **Step 3: Mention PC host in `README.md` Layout and Requirements**

Add `platforms/pc/` to the tree and note CMake + PSY-Z for PC bring-up.

- [ ] **Step 4: Commit**

```bash
git add AGENTS.md README.md
git commit -m "$(cat <<'EOF'
docs: add platforms/pc and vendor/psyz to layout for Phase 1

EOF
)"
```

---

### Task 2: Switch host skeleton (`.nro`)

**Files:**
- Create: `platforms/switch/Makefile`
- Create: `platforms/switch/source/main.c`
- Create: `platforms/switch/README.md`
- Modify: `platforms/switch/README.md` (replace placeholder)

**Interfaces:**
- Consumes: libnx via `DEVKITPRO`
- Produces: `platforms/switch/dw3-switch.nro` when built; status text on console; exit on `+`

- [ ] **Step 1: Write `platforms/switch/source/main.c`**

```c
#include <stdio.h>
#include <switch.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    printf("dw3-switch Phase 1 skeleton\n");
    printf("Press + to exit\n");
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            break;
        }
        if (kDown != 0) {
            printf("buttons_down=0x%llx\n", (unsigned long long)kDown);
            consoleUpdate(NULL);
        }
    }

    consoleExit(NULL);
    return 0;
}
```

- [ ] **Step 2: Write `platforms/switch/Makefile`**

Use the standard libnx homebrew template shape:

```makefile
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
TARGET		:=	dw3-switch
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_SRC)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS	:=	$(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
```

If the template from a current libnx example differs slightly, prefer the working example from `/opt/devkitpro/examples/switch/graphics/simple` inside the Docker image, but keep `TARGET := dw3-switch` and `SOURCES := source`.

- [ ] **Step 3: Write `platforms/switch/README.md`**

Document:

```bash
# from repo root
./tools/dk.sh make -C platforms/switch
# artifact: platforms/switch/dw3-switch.nro
```

Note: no game data required for this skeleton.

- [ ] **Step 4: Commit (build verified in Task 3)**

```bash
git add platforms/switch/Makefile platforms/switch/source/main.c platforms/switch/README.md
git commit -m "$(cat <<'EOF'
feat(switch): add Phase 1 libnx skeleton .nro

EOF
)"
```

---

### Task 3: `tools/dk.sh` Docker Switch builds (P1.1)

**Files:**
- Create: `tools/dk.sh`
- Create: `tools/docker/README.md`

**Interfaces:**
- Consumes: Docker image `devkitpro/devkita64:latest`; repo at `/dw3-switch`
- Produces: `tools/dk.sh <cmd...>` runs with `DEVKITPRO=/opt/devkitpro` and uid/gid of the host user

- [ ] **Step 1: Write `tools/dk.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64:latest}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 1
fi

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <command> [args...]" >&2
  echo "example: $0 make -C platforms/switch" >&2
  exit 1
fi

docker pull "$IMAGE" >/dev/null
exec docker run --rm -t \
  -u "$(id -u):$(id -g)" \
  -e DEVKITPRO=/opt/devkitpro \
  -e DEVKITARM=/opt/devkitpro/devkitARM \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
  -v "$ROOT:/dw3-switch" \
  -w /dw3-switch \
  "$IMAGE" \
  "$@"
```

- [ ] **Step 2: Make executable and document**

```bash
chmod +x tools/dk.sh
```

`tools/docker/README.md`:

```markdown
# Docker images

| Image | Use |
|---|---|
| `devkitpro/devkita64:latest` | Switch homebrew builds via `tools/dk.sh` |
| `vendor/dw3_decomp` Dockerfile | Matching/decomp builds (optional; not required for Phase 1 hosts) |

Override the Switch image with `DW3_DK_IMAGE=...`.
```

- [ ] **Step 3: Build the Switch skeleton**

```bash
./tools/dk.sh make -C platforms/switch clean
./tools/dk.sh make -C platforms/switch
ls -la platforms/switch/dw3-switch.nro
```

Expected: `dw3-switch.nro` exists and size > 0.

- [ ] **Step 4: Commit**

```bash
git add tools/dk.sh tools/docker/README.md
git commit -m "$(cat <<'EOF'
feat(tools): add dk.sh wrapper for devkitA64 Docker builds

EOF
)"
```

---

### Task 4: Vendor PSY-Z (P1.2)

**Files:**
- Create: `vendor/psyz/` (submodule)
- Modify: `.gitmodules`
- Create: `docs/PSYZ.md`

**Interfaces:**
- Consumes: https://github.com/Xeeynamo/psyz.git @ `fac9bd58ccd8a0c35e31c6354528f9d625e9bdd1`
- Produces: `vendor/psyz` recursively initialized (includes `external/SDL`)

- [ ] **Step 1: Install host build deps if missing**

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build pkg-config libgl1-mesa-dev build-essential git
cmake --version   # expect >= 3.21 (Ubuntu 24.04 ships 3.28)
```

- [ ] **Step 2: Add submodule at the pinned commit**

```bash
cd /mnt/windows/Jogos/dw3-switch
git submodule add https://github.com/Xeeynamo/psyz.git vendor/psyz
cd vendor/psyz
git checkout fac9bd58ccd8a0c35e31c6354528f9d625e9bdd1
git submodule update --init --recursive
cd ../..
```

If `git submodule add` fails because the directory exists, fix `.gitmodules` manually and `git submodule absorbgitdirs` / re-init — do not vendor a tarball copy.

- [ ] **Step 3: Build PSY-Z `samples/gpu_hello` from our tree**

```bash
cmake -S vendor/psyz/samples/gpu_hello -B build/psyz-gpu-hello -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/psyz-gpu-hello -j"$(nproc)"
test -x build/psyz-gpu-hello/hello
```

Expected: binary linked successfully. Manual smoke (owner/agent with display): run `./build/psyz-gpu-hello/hello` briefly and quit. Headless CI may skip the run; the link step is required.

- [ ] **Step 4: Write `docs/PSYZ.md`**

Include: pin SHA, MIT license pointer (`vendor/psyz/LICENSE`), renderer default `sdl3_gpu`, recursive submodule note (`external/SDL`), and that Switch has no public SDL3 package (backend TBD in Phase 4).

- [ ] **Step 5: Commit**

```bash
git add .gitmodules vendor/psyz docs/PSYZ.md
git commit -m "$(cat <<'EOF'
feat: vendor PSY-Z submodule at fac9bd58 for PC Psy-Q shim

EOF
)"
```

---

### Task 5: PC host skeleton (P1.3)

**Files:**
- Create: `platforms/pc/CMakeLists.txt`
- Create: `platforms/pc/src/main.c`
- Create: `platforms/pc/README.md`
- Modify: `.gitignore` (ensure `/build/`)

**Interfaces:**
- Consumes: `vendor/psyz` via `add_subdirectory`; `Psyz_QuitRequested`, libgpu/libetc
- Produces: `build/pc/dw3-pc` executable

- [ ] **Step 1: Write failing smoke script expectation**

Document in README that this must work:

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
test -x build/pc/dw3-pc
```

- [ ] **Step 2: Write `platforms/pc/src/main.c`**

Minimal PSY-Z stub (adapted from `samples/gpu_hello`, closes on quit):

```c
#include <psyz.h>
#include <libgpu.h>
#include <libetc.h>
#include <stdio.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OTSIZE 1

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
} DB;

static DB db[2];
static DB *cdb;
u_long _ramsize = 0x00200000;
u_long _stacksize = 0x00004000;

static void init_graphics(void) {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetVideoMode(MODE_NTSC);
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearOTag(db[0].ot, OTSIZE);
    ClearOTag(db[1].ot, OTSIZE);
    SetDispMask(1);
    cdb = &db[0];
}

static void present(void) {
    cdb = (cdb == &db[0]) ? &db[1] : &db[0];
    DrawSync(0);
    VSync(0);
    ClearImage(&cdb->draw.clip, 16, 32, 64);
    DrawOTag(&cdb->ot[0]);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int frame = 0;
    init_graphics();
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(8, 16, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));
    printf("dw3-pc Phase 1 skeleton\n");
    while (!Psyz_QuitRequested()) {
        FntPrint("dw3-pc Phase 1\n");
        FntPrint("frame %d\n", frame++);
        FntPrint("close window to quit\n");
        FntFlush(-1);
        present();
    }
    return 0;
}
```

- [ ] **Step 3: Write `platforms/pc/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.21)
project(dw3-pc C)

set(CMAKE_C_STANDARD 11)
set(PSYZ_RENDERER "sdl3_gpu" CACHE STRING "PSY-Z renderer")

add_subdirectory(${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz ${CMAKE_BINARY_DIR}/psyz)

add_executable(dw3-pc src/main.c)
target_link_libraries(dw3-pc PRIVATE psyz)
target_include_directories(psyz PRIVATE ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
psyz_title(dw3-pc "dw3-pc Phase 1")
```

If PSY-Z's `psyz_title` / include paths differ at the pinned commit, adjust to match `vendor/psyz/samples/gpu_hello/CMakeLists.txt` exactly (that file is the source of truth).

- [ ] **Step 4: Build and verify**

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
test -x build/pc/dw3-pc
```

Expected: exit code 0, binary present.

- [ ] **Step 5: Commit**

```bash
git add platforms/pc/CMakeLists.txt platforms/pc/src/main.c platforms/pc/README.md .gitignore
git commit -m "$(cat <<'EOF'
feat(pc): add PSY-Z Phase 1 host skeleton

EOF
)"
```

---

### Task 6: `build_file_map.py` + unit tests (P1.5 core)

**Files:**
- Create: `tools/build_file_map.py`
- Create: `tools/tests/test_build_file_map.py`
- Create: `tools/testdata/make_mini_iso.py` (generator checked in; regenerates fixture)
- Create: `tools/testdata/mini_iso.bin` (generated, committed — synthetic, not game data)
- Create: `tools/testdata/file_table_us_fixture.c`

**Interfaces:**
- Consumes: MODE2/2352 `.bin`, C snippet or path to `file_table.c` with `VERSION_US` arrays
- Produces: JSON object:

```json
{
  "version": "us",
  "executable": "SLUS_014.36",
  "files": {
    "0": {"path": "AAA/DAT/EXAMPLE.DAT", "lba": 1611, "sectors": 1, "bytes": 2048, "form": 1}
  }
}
```

CLI:

```text
python3 tools/build_file_map.py --bin DISC.bin --version us \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c \
  --out disks/us/files_us.json
python3 tools/build_file_map.py --check disks/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

`--check` exits non-zero if any non-zero `FILE_SECTORS[id]` lacks a path (except documentable holes listed in `--allow-missing`).

- [ ] **Step 1: Write the failing unit test**

`tools/tests/test_build_file_map.py`:

```python
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "build_file_map.py"
ISO = ROOT / "tools" / "testdata" / "mini_iso.bin"
FT = ROOT / "tools" / "testdata" / "file_table_us_fixture.c"


def test_maps_lba_to_path(tmp_path):
    out = tmp_path / "files_us.json"
    r = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--bin",
            str(ISO),
            "--version",
            "us",
            "--file-table",
            str(FT),
            "--out",
            str(out),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    data = json.loads(out.read_text())
    assert data["version"] == "us"
    assert data["files"]["1"]["path"].endswith("DUMMY.DAT")
    assert data["files"]["1"]["lba"] == 22
    assert data["files"]["1"]["sectors"] == 1


def test_check_passes_on_complete_map(tmp_path):
    out = tmp_path / "files_us.json"
    subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--bin",
            str(ISO),
            "--version",
            "us",
            "--file-table",
            str(FT),
            "--out",
            str(out),
        ],
        check=True,
    )
    r = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--check",
            str(out),
            "--file-table",
            str(FT),
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr
```

- [ ] **Step 2: Run tests — expect FAIL (tool missing)**

```bash
python3 -m pytest tools/tests/test_build_file_map.py -v
```

Expected: FAIL (file not found or import error).

- [ ] **Step 3: Create synthetic ISO + fixture C**

`tools/testdata/file_table_us_fixture.c` (minimal, only ids 0 and 1):

```c
#if VERSION_US
s32 FILE_SECTORS[2] = {
    0,
    22,
};
u16 FILE_SECTOR_COUNTS[2] = {
    0,
    1,
};
#endif
```

`tools/testdata/make_mini_iso.py` must write a MODE2/2352 image with PVD at LBA 16, a path table listing root + one file `DUMMY.DAT` at LBA 22, size 2048. Keep the generator deterministic and commit both the script and `mini_iso.bin`.

- [ ] **Step 4: Implement `tools/build_file_map.py`**

Requirements for the implementation:

1. Read MODE2/2352 sectors (`SECTOR=2352`, user data offset `24`), same as `vendor/dw3_decomp/tools/extract_disc.py`.
2. Walk the ISO9660 path table and directory records → `{lba: {path, bytes}}`.
3. Parse `FILE_SECTORS` / `FILE_SECTOR_COUNTS` for the selected version block (`#if VERSION_US` … `#elif` / `#endif`) from the file-table C source with a small state-machine parser (not `cpp`).
4. Join on LBA. Skip `FILE_SECTORS[id] == 0`.
5. Set `"form": 1` for normal files; if path ends with `.STR` (case-insensitive), set `"form": 2` and note in docs that Phase 3 movies need raw-sector packing (pack step may still copy 2048-byte user data in Phase 1).
6. Write JSON with stable key order (`version`, `executable`, `files`) and integer keys as decimal strings.

Include `argparse` with `--bin`, `--version {us,eu}`, `--file-table`, `--out`, `--check`, `--allow-missing ID` (repeatable).

- [ ] **Step 5: Run tests — expect PASS**

```bash
python3 -m pytest tools/tests/test_build_file_map.py -v
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add tools/build_file_map.py tools/tests/test_build_file_map.py \
  tools/testdata/make_mini_iso.py tools/testdata/mini_iso.bin \
  tools/testdata/file_table_us_fixture.c
git commit -m "$(cat <<'EOF'
feat(tools): add build_file_map.py with synthetic ISO tests

EOF
)"
```

---

### Task 7: Owner disc docs + real `files_us.json` generation (P1.5 owner path)

**Files:**
- Create: `docs/DISC_SETUP.md`
- Modify: `.gitignore` if needed (`/disks/` already present)

**Interfaces:**
- Consumes: owner US `.bin`; decomp `extract_disc.py` and `file_table.c`
- Produces: local (gitignored) `disks/us/` tree + `disks/us/files_us.json`

- [ ] **Step 1: Write `docs/DISC_SETUP.md`**

Document exactly:

```bash
# from repo root, with venv optional
python3 vendor/dw3_decomp/tools/extract_disc.py \
  "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  disks/us

sha1sum disks/us/SLUS_014.36
# expect: 444653259f78ddb483fd22af72cce9276f42f214

python3 tools/build_file_map.py \
  --bin "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  --version us \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c \
  --out disks/us/files_us.json

python3 tools/build_file_map.py \
  --check disks/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

Note the cue filename mismatch (`Digimon World 3 (USA).bin` vs actual `Digimon World 3.bin`): always pass the `.bin` path directly.

- [ ] **Step 2: Run the extract + map on the owner's disc (local only)**

```bash
python3 vendor/dw3_decomp/tools/extract_disc.py \
  "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  disks/us
python3 tools/build_file_map.py \
  --bin "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  --version us \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c \
  --out disks/us/files_us.json
python3 tools/build_file_map.py \
  --check disks/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

Expected: `--check` exits 0, or prints a short allow-list of holes that you then either fix in the tool or document with `--allow-missing` + reason in `DISC_SETUP.md`.

- [ ] **Step 3: Verify git does not see disc data**

```bash
git status --short | rg -i 'disks/|AAA/|\.bin' || echo 'clean of disc artifacts'
```

Expected: no staged/untracked disc payloads (untracked `disks/` should be ignored).

- [ ] **Step 4: Commit docs only**

```bash
git add docs/DISC_SETUP.md .gitignore
git commit -m "$(cat <<'EOF'
docs: US disc extract and file-id map instructions

EOF
)"
```

---

### Task 8: Runtime layout + `pack_runtime.py` (P1.6)

**Files:**
- Create: `tools/pack_runtime.py`
- Create: `tools/tests/test_pack_runtime.py`
- Create: `docs/RUNTIME_LAYOUT.md`
- Modify: `.gitignore` (`/runtime/`)

**Interfaces:**
- Consumes: `disks/us/` (extract output) + `disks/us/files_us.json`
- Produces: `runtime/us/` with:

```text
runtime/us/
  files_us.json
  SLUS_014.36
  boot_tim.bin          # exe tail / boot image bytes if separable; else copy noted in docs
  AAA/...               # tree copied from disks/us/AAA
```

PC and Switch share the same relative layout; Switch SD path is `sdmc:/switch/dw3/us/` (document only in Phase 1).

- [ ] **Step 1: Write failing test with tmp dirs**

Test that `pack_runtime.py` copies `AAA/PRO/X.PRO` and writes `files_us.json` into the out dir; run `build_file_map.py --check` on the packed JSON.

- [ ] **Step 2: Implement `pack_runtime.py`**

```text
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
```

Must:

1. Refuse to run if `files_us.json` missing (tell user to run Task 7).
2. Copy `AAA/` recursively.
3. Copy `SLUS_014.36`.
4. Copy `files_us.json`.
5. For boot image: if `assets`/tail is unavailable without a decomp build, write a README stub file `runtime/us/BOOT_IMAGE.txt` explaining it comes from the executable tail at `OVERLAY_VRAM` (filled in Phase 2). Do **not** invent binary bytes.
6. Print summary counts.

- [ ] **Step 3: Document layout in `docs/RUNTIME_LAYOUT.md`**

Include PC path `runtime/us/` and Switch `sdmc:/switch/dw3/us/`.

- [ ] **Step 4: Run pack on owner data (local)**

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
python3 tools/build_file_map.py --check runtime/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

- [ ] **Step 5: Commit tools + docs (not `runtime/`)**

```bash
git add tools/pack_runtime.py tools/tests/test_pack_runtime.py \
  docs/RUNTIME_LAYOUT.md .gitignore
git commit -m "$(cat <<'EOF'
feat(tools): add pack_runtime.py and runtime layout docs

EOF
)"
```

---

### Task 9: GitHub Actions CI (P1.7)

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: repo + Docker Hub `devkitpro/devkita64`
- Produces: green workflow building PC stub (cmake) and Switch `.nro` (`dk.sh` equivalent)

- [ ] **Step 1: Write `.github/workflows/ci.yml`**

```yaml
name: ci

on:
  push:
    branches: [main, phase1-toolchains-hosts]
  pull_request:

jobs:
  pc:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install deps
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build pkg-config libgl1-mesa-dev
      - name: Configure & build PC host
        run: |
          cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
          cmake --build build/pc -j"$(nproc)"
          test -x build/pc/dw3-pc
      - name: Unit tests (tools)
        run: |
          python3 -m pip install pytest
          python3 -m pytest tools/tests -v

  switch:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Build .nro in devkitA64
        run: |
          docker run --rm \
            -e DEVKITPRO=/opt/devkitpro \
            -v "$PWD:/dw3-switch" -w /dw3-switch \
            devkitpro/devkita64:latest \
            make -C platforms/switch
          test -f platforms/switch/dw3-switch.nro
```

- [ ] **Step 2: Push branch and confirm CI (owner network)**

```bash
git add .github/workflows/ci.yml
git commit -m "$(cat <<'EOF'
ci: build PC PSY-Z stub and Switch skeleton on every push

EOF
)"
git push -u origin phase1-toolchains-hosts
```

Expected: both jobs green. If the remote rejects submodule recursion, fix checkout options; do not disable the PC job.

---

### Task 10: Phase 1 gate checklist + ROADMAP update

**Files:**
- Modify: `docs/ROADMAP.md`
- Modify: `README.md` Status line

**Interfaces:**
- Consumes: artifacts from Tasks 2–9
- Produces: Phase 1 items marked done when evidence exists

- [ ] **Step 1: Verify gate locally**

Checklist (all must pass):

1. `./tools/dk.sh make -C platforms/switch` → `.nro`
2. `cmake -S platforms/pc -B build/pc && cmake --build build/pc` → `dw3-pc`
3. `pytest tools/tests -v` → PASS
4. `disks/us/files_us.json` exists locally and `--check` passes
5. `runtime/us/` packed locally (gitignored)
6. CI green on the branch

- [ ] **Step 2: Update `docs/ROADMAP.md` Phase 1 boxes to `[x]` for completed items**

- [ ] **Step 3: Commit**

```bash
git add docs/ROADMAP.md README.md
git commit -m "$(cat <<'EOF'
docs: mark Phase 1 host and data-pipeline gate complete

EOF
)"
```

---

## Self-review (plan author)

1. **Spec coverage:** ROADMAP P1.1–P1.7 each map to Tasks 2–10. Disc SHA / US version locked in Global Constraints.
2. **Placeholders:** none intentional; boot TIM binary deferred explicitly to Phase 2 with a text stub.
3. **Type consistency:** JSON schema keys (`version`, `files`, `lba`, `sectors`, `bytes`, `form`) used the same in Tasks 6–8.

## Out of scope (do not do in this plan)

- Linking `vendor/dw3_decomp` game C
- PSY-Z libnx backend
- Overlay loader / memcard / cdfs glue (`port/`)
- Committing any disc bytes
