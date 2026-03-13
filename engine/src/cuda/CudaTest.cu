/* Start Header *****************************************************************/
/*!
\file   CudaTest.cpp
\author Choi Meng Yew (100%)
\date   13th March 2026
\brief
Provides a simple CUDA diagnostic test used to verify that CUDA is
properly installed and functioning in the GrapeEngine environment.

The test performs the following steps:
    - Allocates device memory
    - Launches a CUDA kernel
    - Copies results back to host memory
    - Verifies correctness of the computation

This utility is intended for engine users or developers to confirm that
the CUDA runtime, device driver, and kernel execution pipeline are
working correctly before enabling CUDA-based engine systems such as
GPU boids or particle simulation.

Responsibilities:
    - Validate CUDA device memory allocation
    - Launch and execute a simple CUDA kernel
    - Verify device-to-host memory transfers
    - Provide pass/fail diagnostic output

Used by:
    - Engine startup validation
    - CUDA feature detection
    - Debugging CUDA installation issues

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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