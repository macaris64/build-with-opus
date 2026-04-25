# cFS Runtime — Vendor Submodules

This directory hosts the NASA cFS runtime (cFE + OSAL + PSP) as git submodules.
The submodules are not checked in — initialize them before building with
`SAKURA_CFS_RUNTIME=ON`:

```bash
git submodule update --init --recursive
```

Or, to add them from scratch:

```bash
git submodule add https://github.com/nasa/cFE.git    cfs/cFE
git submodule add https://github.com/nasa/osal.git   cfs/osal
git submodule add https://github.com/nasa/PSP.git    cfs/PSP
git submodule update --init --recursive
```

## Build with cFS Runtime

```bash
cmake -B build_cfs \
    -DSAKURA_CFS_RUNTIME=ON \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build_cfs
```

Each SAKURA-II cFS app is built as a shared library (`lib<app>.so`) that can
be loaded by `core-cpu1` at startup. See `cpu1/startup_scripts/cfe_es_startup.scr`.

## Without the Submodules

The standard build (`SAKURA_CFS_RUNTIME=OFF`, the default) continues to work
with CMocka unit tests only — no cFE runtime required.
