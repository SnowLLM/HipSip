#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime_api.h>

#include "hipsip_table.h"

static const struct hipsip_table *tbl;

static void fail(const char *why) {
    fprintf(stderr, "amdhip64 (HipSip): %s\n", why);
    fflush(stderr);
    ExitProcess(3);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)inst;
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    char buf[32];
    if (!GetEnvironmentVariableA("HIPSIP_TABLE", buf, sizeof buf))
        fail("HIPSIP_TABLE is not set -- run through HipSip's run.sh");
    void *address = NULL;
    if (sscanf(buf, "%p", &address) != 1 || !address)
        fail("HIPSIP_TABLE does not parse as a pointer");
    tbl = address;
    if (tbl->magic != HIPSIP_TABLE_MAGIC)
        fail("bridge table magic mismatch -- the two halves are from different builds");
    if (tbl->count != IDX_COUNT)
        fail("bridge table holds a different number of entry points than this DLL forwards");
    return TRUE;
}

#define __hipRegisterFatBinary __hipRegisterFatBinary_forward
#define HIP_FN(ret, name, params, args)                                                            \
    ret name params { return ((ret(*) params)tbl->fn[IDX_##name]) args; }
#include "hip_symbols.def"
#undef HIP_FN
#undef __hipRegisterFatBinary

// HIPK moves device code into sidecar archives. Linux cannot discover the filename
// of Wine's anonymous PE mappings, so discover it on the Windows side instead.
void **__hipRegisterFatBinary(const void *data) {
    typedef void **(*register_fn)(const void *, const char *);
    const unsigned int *words = (const unsigned int *)data;
    if (!data || words[0] != 0x4b504948u)
        return ((register_fn)tbl->fn[IDX___hipRegisterFatBinary])(data, NULL);

    HMODULE module = NULL;
    WCHAR wide[32768];
    char path[131072];
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)data, &module))
        fail("cannot find the module containing HIPK metadata");
    DWORD n = GetModuleFileNameW(module, wide, sizeof wide / sizeof wide[0]);
    if (!n || n >= sizeof wide / sizeof wide[0] ||
        !WideCharToMultiByte(CP_UTF8, 0, wide, -1, path, sizeof path, NULL, NULL))
        fail("cannot read the filename containing HIPK metadata");
    return ((register_fn)tbl->fn[IDX___hipRegisterFatBinary])(data, path);
}
