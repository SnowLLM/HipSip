#pragma once

#include <stdint.h>

#define HIPSIP_TABLE_MAGIC UINT64_C(0x4849505349503031) /* HIPSIP01 */

enum {
#define HIP_FN(ret, name, params, args) IDX_##name,
#include "hip_symbols.def"
#undef HIP_FN
    IDX_COUNT
};

struct hipsip_table {
    uint64_t magic;
    uint64_t count;
    void *fn[IDX_COUNT];
};
