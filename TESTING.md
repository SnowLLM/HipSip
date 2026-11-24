# Testing HipSip

## Run the included checks

From the source root, set `XWIN` to the MSVC sysroot used for the build. The
default check runs the Windows memory probe. Set `GPU_ARCH` to your GPU target
to also compile and launch the bundled Windows HIP kernel.

```sh
make check XWIN="$HOME/xwin"
make check XWIN="$HOME/xwin" GPU_ARCH=gfx1151
```

The kernel launches a two-dimensional grid, writes an index per thread and
verifies every value on the host. This exercises fat binary registration and
the launch ABI. Set `OUT` to use a separate build directory. On shared
hardware, invoke `make check` through the host GPU scheduler.

## Run existing Windows kernel tests

Put the `test_*.exe` files and their dependencies in a writable test directory.
The sweep starts a fresh Wine process for each executable. If the tests take a
fixture directory, pass its Linux path as the second argument. The runner
converts it using the active Wine prefix. Run separate sweeps for groups that
need different fixtures.

```sh
./sweep.sh ./kernel-tests
./sweep.sh ./attention-tests ./fixtures/attention
```

The summary reports passes, failures and skips. A failure makes the sweep exit
with a nonzero status. Exit 77 indicates a skip. A test that exits 2 and prints
its `<fixture_dir>` usage when fixtures were omitted also counts as skipped.
For per-test GPU locking, set `HIPSIP_TEST_WRAPPER` to the scheduler executable.
On a host with withgpu, use
`HIPSIP_TEST_WRAPPER="$HOME/withgpu/withgpu" ./sweep.sh ./kernel-tests`.

## Check a Windows PyTorch installation

With a Windows ROCm build of PyTorch installed, set `PY` to the Linux path of
that environment's `python.exe`. The check multiplies two 32 × 32 BF16 matrices
on the GPU and compares every element on the CPU. Some ROCm wheels load
`Lib/site-packages/_rocm_sdk_core/bin/amdhip64_7.dll` from their own directory.
For that layout, use a disposable Windows Python installation and copy the
matching `amdhip64_7.dll` from the HipSip build into that location before
running the check. The launcher installs DLLs beside `python.exe` only.

```sh
./run.sh "$PY" "$(winepath -w "$PWD/tests/torch_smoke.cu")"
```

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Wine reports a missing HIP entry point | Inspect the executable and its dependent DLLs with `objdump -p file.exe`. Compare their HIP imports and the requested DLL name with the [supported exports](hip_symbols.def). |
| HipSip reports that `libamdhip64.so` is missing a symbol | Check which Linux ROCm library the dynamic loader finds. HipSip resolves the full supported symbol list on the first HIP call. |
| `HIPSIP_TABLE is not set` | Start the executable with `run.sh` so Wine loads the Unix library. |
| The bridge table magic or entry count differs | Use the Windows DLLs and Linux library from one build. For a custom output directory, use the same `OUT` for Make and the launcher. |
| The probe passes but a GPU kernel fails to load | Check the GPU target of the application, any code archives shipped alongside its DLLs, and drive mappings in the active Wine prefix. The probe exercises allocation and copies. |

To see which HIP calls reach the host, enable tracing for one run:

```sh
HIPSIP_TRACE=1 ./run.sh build/hip_probe.exe
```

For Wine loader diagnostics, add `WINEDEBUG=+loaddll` to the launch command.
