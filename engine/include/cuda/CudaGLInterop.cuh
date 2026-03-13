/* Start Header *****************************************************************/
/*!
\file   CudaGLInterop.cuh
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

#pragma once

#ifdef GRAPE_HAS_CUDA

#include <glad/glad.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

namespace CudaGL {

    // Register an existing OpenGL buffer with CUDA for interop.
    // Returns a CUDA graphics resource handle. The buffer can then be
    // mapped/unmapped each frame for kernel access.
    cudaGraphicsResource_t RegisterBuffer(GLuint glBuffer);

    // Unregister a previously registered buffer.
    void UnregisterBuffer(cudaGraphicsResource_t resource);

    // Map a registered GL buffer and return a device pointer.
    // The buffer is locked for CUDA access until Unmap() is called.
    // OpenGL must NOT touch this buffer while it is mapped.
    template<typename T>
    T* Map(cudaGraphicsResource_t resource, size_t* numBytes = nullptr);

    // Unmap a previously mapped buffer, releasing it back to OpenGL.
    void Unmap(cudaGraphicsResource_t resource);

} // namespace CudaGL

#endif // GRAPE_HAS_CUDA
