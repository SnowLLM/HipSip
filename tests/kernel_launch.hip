#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>

constexpr unsigned grid_x = 2, grid_y = 3;
constexpr unsigned block_x = 8, block_y = 2;

__global__ void write_indices(unsigned *output) {
    if (gridDim.x != grid_x || gridDim.y != grid_y || gridDim.z != 1 ||
        blockDim.x != block_x || blockDim.y != block_y || blockDim.z != 1)
        return;
    unsigned x = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned y = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned i = y * (gridDim.x * blockDim.x) + x;
    output[i] = i + 1;
}

static void check(hipError_t status, const char *operation) {
    if (status != hipSuccess) {
        std::fprintf(stderr, "%s: %s\n", operation, hipGetErrorString(status));
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    const dim3 grid(grid_x, grid_y), block(block_x, block_y);
    unsigned result[grid_x * grid_y * block_x * block_y];
    unsigned *device = nullptr;
    check(hipMalloc(&device, sizeof(result)), "hipMalloc");
    check(hipMemset(device, 0, sizeof(result)), "hipMemset");
    write_indices<<<grid, block>>>(device);
    check(hipGetLastError(), "write_indices");
    check(hipMemcpy(result, device, sizeof(result), hipMemcpyDeviceToHost), "hipMemcpy");
    check(hipFree(device), "hipFree");
    for (unsigned i = 0; i < sizeof(result) / sizeof(result[0]); ++i) {
        if (result[i] != i + 1) {
            std::fprintf(stderr, "index %u: expected %u, got %u\n", i, i + 1, result[i]);
            return EXIT_FAILURE;
        }
    }
    std::puts("Windows HIP kernel launch passed");
}
