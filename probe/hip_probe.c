#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pe/hip_compat.h"

#define hipSuccess 0
#define hipMemcpyHostToDevice 1
#define hipMemcpyDeviceToHost 2

__declspec(dllimport) hipError_t hipGetDevice(int *d);
__declspec(dllimport) hipError_t hipMalloc(void **p, size_t n);
__declspec(dllimport) hipError_t hipFree(void *p);
__declspec(dllimport) hipError_t hipMemcpy(void *d, const void *s, size_t n, hipMemcpyKind k);
__declspec(dllimport) hipError_t hipHostMalloc(void **p, size_t n, unsigned flags);
__declspec(dllimport) hipError_t hipHostFree(void *p);
__declspec(dllimport) hipError_t hipDeviceSynchronize(void);
__declspec(dllimport) const char *hipGetErrorString(hipError_t e);

static void check(hipError_t e, const char *what) {
    printf("%-34s %s\n", what, e == hipSuccess ? "ok" : hipGetErrorString(e));
    if (e != hipSuccess)
        exit(EXIT_FAILURE);
}

int main(void) {
    int dev = -1;
    check(hipGetDevice(&dev), "hipGetDevice");
    printf("%-34s %d\n", "  device", dev);

    enum { kN = 1 << 20 };
    void *d = NULL;
    check(hipMalloc(&d, kN), "hipMalloc 1 MiB");
    printf("%-34s %p\n", "  device pointer", d);

    void *pinned = NULL;
    check(hipHostMalloc(&pinned, kN, 0), "hipHostMalloc 1 MiB");

    // The pinned allocation came from the Linux side and is written here by Windows-side code --
    // if the two were not the same address space this is where it would fault.
    unsigned char *src = (unsigned char *)pinned;
    for (int i = 0; i < kN; i++)
        src[i] = (unsigned char)(i * 31 + (i >> 13));

    check(hipMemcpy(d, src, kN, hipMemcpyHostToDevice), "hipMemcpy H2D");
    unsigned char *back = (unsigned char *)malloc(kN); // ordinary PE heap this time
    if (!back) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    memset(back, 0, kN);
    check(hipMemcpy(back, d, kN, hipMemcpyDeviceToHost), "hipMemcpy D2H (into PE heap)");
    check(hipDeviceSynchronize(), "hipDeviceSynchronize");

    int bad = -1;
    for (int i = 0; i < kN; i++)
        if (back[i] != src[i]) {
            bad = i;
            break;
        }
    printf("%-34s %s\n", "1 MiB round trip compares equal", bad < 0 ? "ok" : "FAILED");
    if (bad >= 0) {
        printf("  first mismatch at %d\n", bad);
    }

    free(back);
    check(hipHostFree(pinned), "hipHostFree");
    check(hipFree(d), "hipFree");
    printf("\n%s\n", bad >= 0 ? "FAILED" : "all ok");
    return bad >= 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
