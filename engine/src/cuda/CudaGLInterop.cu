/* Start Header *****************************************************************/
/*!
\file   CudaGLInterop.cu
\author Choi Meng Yew (100%)
\date   03/03/2026 DD/MM/YYYY
\brief
Provides CUDA–OpenGL interoperability utilities for registering,
mapping, and unmapping OpenGL buffer objects (VBOs) for CUDA access.

This module enables zero-copy GPU data sharing between CUDA compute
kernels and OpenGL rendering by wrapping cudaGraphics* APIs.

Responsibilities:
    - Register OpenGL buffer with CUDA
    - Map buffer for CUDA device access
    - Retrieve device pointer
    - Unmap and unregister safely

Used by:
    - CudaBoids
    - CudaParticles
    - Any future GPU compute pipeline
*/
/* End Header *******************************************************************/

#include "cuda/CudaGLInterop.cuh"

#ifdef GRAPE_HAS_CUDA

#include <cstdio>

namespace CudaGL {

    /*!
    \brief
    Registers an OpenGL buffer object with CUDA for interop.

    Associates a GL VBO with CUDA so it can later be mapped and accessed
    directly by CUDA kernels. The buffer must already be created and
    allocated by OpenGL.

    Registered with cudaGraphicsMapFlagsWriteDiscard, meaning CUDA
    will overwrite the entire buffer contents.

    \param glBuffer
    OpenGL buffer ID to register.

    \return
    cudaGraphicsResource_t handle, or nullptr if registration fails.
    */
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

    /*!
    \brief
    Unregisters a CUDA-registered OpenGL buffer.

    Releases the association between CUDA and the GL buffer.
    Should be called during shutdown or when destroying the buffer.

    \param resource
    CUDA graphics resource handle obtained from RegisterBuffer().
    */
    void UnregisterBuffer(cudaGraphicsResource_t resource) {
        if (resource) {
            cudaGraphicsUnregisterResource(resource);
        }
    }

    /*!
    \brief
    Maps a registered OpenGL buffer into CUDA address space.

    Locks the resource and returns a device pointer to the
    underlying GPU memory so CUDA kernels can write directly
    into the buffer.

    Must be paired with Unmap() before OpenGL rendering.

    \tparam T
    Element type of the mapped buffer.

    \param resource
    CUDA graphics resource handle.

    \param numBytes
    Optional output size of the mapped memory in bytes.

    \return
    Device pointer to mapped memory, or nullptr on failure.
    */
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

    /*!
    \brief
    Unmaps a CUDA-mapped OpenGL buffer.

    Releases the resource from CUDA access and returns
    ownership back to OpenGL so it can be used for rendering.

    Must be called after Map() and before issuing
    OpenGL draw calls.

    \param resource
    CUDA graphics resource handle.
    */
    void Unmap(cudaGraphicsResource_t resource) {
        cudaGraphicsUnmapResources(1, &resource, 0);
    }

    // Explicit template instantiation for float4 (boid position+velocity)
    template float4* Map<float4>(cudaGraphicsResource_t, size_t*);

} // namespace CudaGL

#endif // GRAPE_HAS_CUDA
