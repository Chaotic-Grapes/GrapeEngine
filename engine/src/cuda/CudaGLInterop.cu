#include "cuda/CudaGLInterop.cuh"

#ifdef GRAPE_HAS_CUDA

#include <cstdio>

namespace CudaGL {

    cudaGraphicsResource_t RegisterBuffer(GLuint glBuffer) {
        cudaGraphicsResource_t resource = nullptr;
        cudaError_t err = cudaGraphicsGLRegisterBuffer(
            &resource,
            glBuffer,
            cudaGraphicsMapFlagsWriteDiscard  // CUDA will overwrite the entire buffer
        );
        if (err != cudaSuccess) {
            printf("[CudaGL] Failed to register GL buffer %u: %s\n",
                   glBuffer, cudaGetErrorString(err));
            return nullptr;
        }
        return resource;
    }

    void UnregisterBuffer(cudaGraphicsResource_t resource) {
        if (resource) {
            cudaGraphicsUnregisterResource(resource);
        }
    }

    template<typename T>
    T* Map(cudaGraphicsResource_t resource, size_t* numBytes) {
        cudaError_t err = cudaGraphicsMapResources(1, &resource, 0);
        if (err != cudaSuccess) {
            printf("[CudaGL] Failed to map resource: %s\n", cudaGetErrorString(err));
            return nullptr;
        }

        T* devPtr = nullptr;
        size_t size = 0;
        err = cudaGraphicsResourceGetMappedPointer(
            reinterpret_cast<void**>(&devPtr), &size, resource);
        if (err != cudaSuccess) {
            printf("[CudaGL] Failed to get mapped pointer: %s\n", cudaGetErrorString(err));
            cudaGraphicsUnmapResources(1, &resource, 0);
            return nullptr;
        }

        if (numBytes) *numBytes = size;
        return devPtr;
    }

    void Unmap(cudaGraphicsResource_t resource) {
        cudaGraphicsUnmapResources(1, &resource, 0);
    }

    // Explicit template instantiation for float4 (boid position+velocity)
    template float4* Map<float4>(cudaGraphicsResource_t, size_t*);

} // namespace CudaGL

#endif // GRAPE_HAS_CUDA
