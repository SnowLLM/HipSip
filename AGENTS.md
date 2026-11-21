# Working on HipSip

## Bridge ABI

- `pe/amdhip64.c` builds the Windows DLL. `unix/hipsip.c` resolves the host HIP runtime and exposes Win64-callable functions marked `ms_abi`. Keep `dlopen` deferred to the first HIP call so Wine helper processes can start without initializing ROCm.
- `hipsip_table.h` defines the shared table layout. `hip_symbols.def` defines the slot order, signatures and PE exports. Append entries. Bump `HIPSIP_TABLE_MAGIC` for incompatible changes such as reordering slots or changing a signature without changing the count.
- For a new export, keep `HIP_FN(return_type, symbol_name,` on one line so the Makefile can extract the name. Match the ROCm header types on both sides, particularly structs passed by value. Check compiler-generated registration symbols against Clang’s HIP registration ABI.
- Review callbacks and filenames individually. Callbacks cross the calling convention in the opposite direction. Windows drive paths need translation using the active Wine prefix before Linux opens them.
- The PE wrapper for `__hipRegisterFatBinary` passes the source module path to the Unix half for HIPK archives. Keep the decoded bytes alive until `__hipUnregisterFatBinary` releases the corresponding handle.

## Looking up a signature

From the source root, the following example prints the declaration for one HIP symbol. Review the output alongside the existing manifest. Some compiler and extension entry points are absent from this header.

```sh
tmpdir=$(mktemp -d)
printf '#include <hip/hip_runtime_api.h>\n' | "${ROCM_PATH:-/opt/rocm}/llvm/bin/clang" -x c -D__HIP_PLATFORM_AMD__ -I"${ROCM_PATH:-/opt/rocm}/include" -Xclang -ast-dump=json -fsyntax-only - > "$tmpdir/hip_ast.json"
printf 'hipMalloc\n' > "$tmpdir/symbols.txt"
python3 tools/gen_symbols.py "$tmpdir/hip_ast.json" "$tmpdir/symbols.txt"
```

## Verifying a change

- Build both halves and the probe with `make -j XWIN=/path/to/xwin`. Make tracks include dependencies. Set `OUT` when building outside the source tree and pass that directory to the launcher.
- Run the [memory probe](README.md#build-and-try-it) after changing the table or launcher. On hosts with a shared GPU scheduler, launch GPU processes through that scheduler. With withgpu, use `"$HOME/withgpu/withgpu" ./run.sh build/hip_probe.exe`.
- For changes to kernel registration, module loading or callbacks, also run a Windows executable that exercises that specific path. For changes to exports, inspect the built PE DLL with `llvm-readobj --coff-exports`.
- Keep application-specific tests and fixtures with their application. Keep generated binaries, build trees, MSVC sysroots and machine-specific paths out of the source distribution.
