#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <hip/hip_runtime_api.h>

#include "hipsip_table.h"

#define MS_ABI __attribute__((ms_abi))

#define HIP_FN(ret, name, params, args) typedef ret (*name##_fn) params;
#include "hip_symbols.def"
#undef HIP_FN

#define HIP_FN(ret, name, params, args) static name##_fn real_##name;
#include "hip_symbols.def"
#undef HIP_FN

static pthread_once_t once = PTHREAD_ONCE_INIT;
static void *lib;
static int tracing;

static void trace(const char *name) {
    if (tracing)
        fprintf(stderr, "hipsip: %s\n", name);
}

static void load(void) {
    // Wine starts helper processes that may never call HIP.
    lib = dlopen("libamdhip64.so", RTLD_NOW | RTLD_GLOBAL);
    if (!lib) {
        fprintf(stderr, "hipsip: %s\n", dlerror());
        abort();
    }
#define HIP_FN(ret, name, params, args)                                                            \
    real_##name = (name##_fn)dlsym(lib, #name);                                                    \
    if (!real_##name) {                                                                            \
        fprintf(stderr, "hipsip: libamdhip64.so has no %s\n", #name);                           \
        abort();                                                                                   \
    }
#include "hip_symbols.def"
#undef HIP_FN
}

#define HIP_FN(ret, name, params, args)                                                            \
    static ret MS_ABI br_##name params {                                                           \
        pthread_once(&once, load);                                                                 \
        trace(#name);                                                                              \
        return real_##name args;                                                                   \
    }
#include "hip_symbols.def"
#undef HIP_FN

// Resolve drive letters using the active Wine prefix.
static const char *unix_filename(const char *fname, char *unix_path, size_t capacity) {
    if (!fname || !isalpha((unsigned char)fname[0]) || fname[1] != ':' ||
        (fname[2] != '/' && fname[2] != '\\'))
        return fname;

    const char *prefix = getenv("WINEPREFIX");
    const char *base = prefix ? prefix : getenv("HOME");
    if (!base)
        return fname;

    // Keep the drive symlink in the path so relative mappings such as c: -> ../drive_c work.
    int offset = snprintf(unix_path, capacity, "%s%s/dosdevices/%c:", base,
                          prefix ? "" : "/.wine", tolower((unsigned char)fname[0]));
    if (offset < 0 || (size_t)offset >= capacity || strlen(fname + 2) >= capacity - (size_t)offset)
        return fname;
    strcpy(unix_path + offset, fname + 2);
    for (char *c = unix_path + offset; *c; c++)
        if (*c == '\\')
            *c = '/';
    return unix_path;
}

static hipError_t MS_ABI br_hipModuleLoad_translating(hipModule_t *module, const char *fname) {
    pthread_once(&once, load);
    trace("hipModuleLoad");
    char unix_path[131072];
    return real_hipModuleLoad(module, unix_filename(fname, unix_path, sizeof unix_path));
}

// HIPK refers to a device-code archive beside a PE DLL. Load its code object
// using the filename discovered by the PE half, then register ordinary HIPF
// bytes. The real runtime keeps these bytes until the module is unregistered.
struct saved_fatbin {
    struct {
        uint32_t magic, version;
        void *binary;
        void *reserved;
    } wrapper;
    void **handle;
    struct saved_fatbin *next;
};
static struct saved_fatbin *saved_fatbins;
static pthread_mutex_t kpack_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *kpack_cache;
static int (*kpack_create)(void **);
static int (*kpack_load)(void *, const void *, const char *, uint32_t,
                         const char *const *, size_t, void **, size_t *);
static void (*kpack_free)(void *);

static void **MS_ABI br_register_fatbin_path(const void *data, const char *filename) {
    pthread_once(&once, load);
    const struct saved_fatbin *input = data;
    if (!data || input->wrapper.magic != 0x4b504948u)
        return real___hipRegisterFatBinary(data);

    pthread_mutex_lock(&kpack_mutex);
    if (!kpack_cache) {
        kpack_create = dlsym(lib, "kpack_cache_create");
        kpack_load = dlsym(lib, "kpack_load_code_object");
        kpack_free = dlsym(lib, "kpack_free_code_object");
        if (!kpack_create || !kpack_load || !kpack_free || kpack_create(&kpack_cache)) {
            fprintf(stderr, "hipsip: Linux runtime has no usable kpack loader\n");
            abort();
        }
    }
    int device = 0;
    hipDeviceProp_tR0600 prop;
    hipError_t err = real_hipGetDevice(&device);
    if (err == hipSuccess)
        err = real_hipGetDevicePropertiesR0600(&prop, device);
    if (err != hipSuccess || !filename) {
        fprintf(stderr, "hipsip: cannot resolve HIPK device/path (HIP error %d)\n", err);
        abort();
    }
    char unix_path[131072];
    const char *path = unix_filename(filename, unix_path, sizeof unix_path);
    const char *archs[] = {prop.gcnArchName};
    struct saved_fatbin *saved = calloc(1, sizeof *saved);
    size_t size = 0;
    int result = saved ? kpack_load(kpack_cache, input->wrapper.binary, path,
                                    (uint32_t)(uintptr_t)input->wrapper.reserved,
                                    archs, 1, &saved->wrapper.binary, &size) : -1;
    if (result) {
        fprintf(stderr, "hipsip: cannot load HIPK from %s for %s (kpack error %d)\n",
                path, prop.gcnArchName, result);
        abort();
    }
    saved->wrapper.magic = 0x48495046u;
    saved->wrapper.version = 1;
    saved->handle = real___hipRegisterFatBinary(&saved->wrapper);
    if (!saved->handle) {
        fprintf(stderr, "hipsip: cannot register decoded HIPK from %s\n", path);
        abort();
    }
    saved->next = saved_fatbins;
    saved_fatbins = saved;
    pthread_mutex_unlock(&kpack_mutex);
    return saved->handle;
}

static void MS_ABI br_unregister_fatbin_saved(void **handle) {
    pthread_once(&once, load);
    real___hipUnregisterFatBinary(handle);
    pthread_mutex_lock(&kpack_mutex);
    struct saved_fatbin **at = &saved_fatbins;
    while (*at) {
        struct saved_fatbin *saved = *at;
        if (saved->handle == handle) {
            *at = saved->next;
            kpack_free(saved->wrapper.binary);
            free(saved);
            break;
        }
        at = &saved->next;
    }
    pthread_mutex_unlock(&kpack_mutex);
}

static struct hipsip_table table;

__attribute__((constructor)) static void publish(void) {
    tracing = getenv("HIPSIP_TRACE") != NULL;
#define HIP_FN(ret, name, params, args) table.fn[IDX_##name] = (void *)br_##name;
#include "hip_symbols.def"
#undef HIP_FN
    table.fn[IDX_hipModuleLoad] = (void *)br_hipModuleLoad_translating;
    table.fn[IDX___hipRegisterFatBinary] = (void *)br_register_fatbin_path;
    table.fn[IDX___hipUnregisterFatBinary] = (void *)br_unregister_fatbin_saved;
    table.magic = HIPSIP_TABLE_MAGIC;
    table.count = IDX_COUNT;
    char buf[32];
    snprintf(buf, sizeof buf, "%p", (void *)&table);
    setenv("HIPSIP_TABLE", buf, 1);
}
