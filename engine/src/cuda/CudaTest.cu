#include <cuda_runtime.h>
#include <stdio.h>

__global__ void testKernel(float* out, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = idx * 2.0f;
    }
}

bool CudaTestRun() {
    const int N = 256;
    float* d_data = nullptr;
    float h_data[256];

    cudaError_t err = cudaMalloc(&d_data, N * sizeof(float));
    if (err != cudaSuccess) {
        printf("CUDA malloc failed: %s\n", cudaGetErrorString(err));
        return false;
    }

    testKernel<<<1, 256>>>(d_data, N);
    cudaMemcpy(h_data, d_data, N * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(d_data);

    // Verify: h_data[5] should be 10.0
    bool success = (h_data[5] == 10.0f);
    printf("CUDA test: %s (h_data[5] = %.1f)\n", success ? "PASSED" : "FAILED", h_data[5]);
    return success;
}