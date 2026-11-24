# HipSip

HipSip runs 64-bit Windows HIP programs on Linux with Wine. If you have a
Windows executable built for your AMD GPU, you can run it against your Linux
ROCm driver. HipSip supplies the `amdhip64` DLL that the executable expects.

## Build and try it

You need an x86-64 Linux host, an AMD GPU supported by your ROCm installation,
and 64-bit Wine. To build, install GCC, GNU Make 4.3 or newer, and ROCm with
its development headers and LLVM tools (Clang, LLD and llvm-dlltool). The
Windows build also needs the x86-64 MSVC headers and libraries. Set `XWIN` to
the directory containing `crt/` and `sdk/` from an [xwin splat](https://github.com/Jake-Shadle/xwin).

```sh
make -j XWIN="$HOME/xwin"
./run.sh build/hip_probe.exe
```

The probe copies 1 MiB of data to the GPU and back and checks every byte.
Look for `all ok` on the last line. If ROCm is installed outside `/opt/rocm`,
pass `ROCM_PATH=/path/to/rocm` to Make and make sure the Linux dynamic loader
can find that installation’s `libamdhip64.so`.

## Run your application

Put a copy of your Windows application in a writable directory, along with its
dependent DLLs and GPU code archives. The launcher **overwrites matching HIP
DLLs beside the executable**. Pass the executable’s Linux path and any
arguments the application expects.

```sh
./run.sh ./my-app/example.exe
```

Wine runs the Windows executable and the Linux library in the same process.
For example, when the application calls `hipMemcpy`, HipSip forwards the call
to Linux ROCm with the original pointers. Device code runs on the GPU, so the
application needs code compiled for your GPU architecture.

HipSip provides `amdhip64.dll`, `amdhip64_6.dll` and `amdhip64_7.dll`.
Compatibility depends on the HIP functions imported by the application and its
dependencies. The [symbol list](hip_symbols.def) shows the supported entry
points. See [Testing HipSip](TESTING.md) for kernel launches, existing Windows test suites,
PyTorch checks and loader diagnostics.

## License

HipSip is licensed under the [Apache License, Version 2.0](LICENSE).
