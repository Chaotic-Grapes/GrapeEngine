/* Start Header *****************************************************************/
/*!
\file   CudaGLInterop.cuh
\author Choi Meng Yew (100%)
\date   03/03/2026 DD/MM/YYYY
\brief
Provides CUDA�OpenGL interoperability utilities for registering,
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

#pragma once

#ifdef GRAPE_HAS_CUDA

#include <glad/glad.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

namespace CudaGL {

    /**
     * @brief Register an existing OpenGL buffer with CUDA for interoperability.
     *        The returned handle can be mapped and unmapped each frame for kernel access.
     * @param glBuffer OpenGL buffer object ID to register.
     * @return CUDA graphics resource handle for the registered buffer.
     */
    cudaGraphicsResource_t RegisterBuffer(GLuint glBuffer);

    /**
     * @brief Unregister a previously registered CUDA-GL buffer.
     * @param resource CUDA graphics resource handle returned by RegisterBuffer.
     */
    void UnregisterBuffer(cudaGraphicsResource_t resource);

    /**
     * @brief Map a registered GL buffer and return a typed device pointer.
     *        The buffer is locked for CUDA access until Unmap() is called.
     *        OpenGL must NOT touch this buffer while it is mapped.
     * @tparam T Element type to cast the device pointer to.
     * @param resource CUDA graphics resource handle returned by RegisterBuffer.
     * @param numBytes Optional output pointer receiving the mapped buffer size in bytes.
     * @return Typed device pointer to the mapped buffer memory.
     */
    template<typename T>
    T* Map(cudaGraphicsResource_t resource, size_t* numBytes = nullptr);

    /**
     * @brief Unmap a previously mapped buffer, releasing it back to OpenGL.
     * @param resource CUDA graphics resource handle that was mapped.
     */
    void Unmap(cudaGraphicsResource_t resource);

} // namespace CudaGL

#endif // GRAPE_HAS_CUDA
