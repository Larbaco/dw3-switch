# CI submodule initialization

The PC job previously used recursive `actions/checkout`, which fetched
`vendor/psyz/nugget/third_party/xmake-psx` from Codeberg. PR #99 run
`38277222737` and rerun job `114906557435` failed with HTTP 503 during
checkout, before compilation. That toolchain builds matching PS1 binaries;
neither native host uses it.

The PC and Switch game jobs now initialize the pinned `vendor/dw3_decomp`
and `vendor/psyz` checkouts, then only `nugget` and `external/SDL` inside
PSY-Z, without `--recursive`. Both game hosts compile
`nugget/bsdec/bsdec.c` through `port/CMakeLists.txt` for movies. Omitting
nugget from the PC job would therefore break configuration.

The Switch sample uses no submodules. Switch game and release workflows
already used this explicit initialization; their dependency paths stay the
same. No origins, gitlink pins, compiler requirements, or build/test steps
change. Required dependency checkout failures still fail the job.
