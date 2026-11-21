# Troubleshooting

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
