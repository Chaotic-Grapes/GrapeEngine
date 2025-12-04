/* Start Header *****************************************************************/
/*!
\file   PixelBufferObject.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Declares the PixelBufferObject (PBO) class, which encapsulates an OpenGL
pixel buffer object used for asynchronous pixel transfers between the GPU
and CPU.

The PixelBufferObject class provides functions to:
- Create, bind, and unbind GPU buffer objects for pixel data operations.
- Map and unmap buffer memory for CPU-side access to GPU data.
- Efficiently read back color or picking data without stalling the GPU.
- Manage buffer lifetime through move semantics and explicit destruction.

This class is primarily used in the engine’s object picking system, enabling
non-blocking entity ID readbacks from off-screen framebuffers via double-buffered
asynchronous PBOs.
*/
/* End Header *******************************************************************/

#pragma once

#include <glad/glad.h>
#include <cstdint>

class PixelBufferObject
{
public:
    PixelBufferObject() = default;
    ~PixelBufferObject();

    // Disable copy
    PixelBufferObject(const PixelBufferObject&) = delete;
    PixelBufferObject& operator=(const PixelBufferObject&) = delete;

    // Enable move
    PixelBufferObject(PixelBufferObject&& other) noexcept;
    PixelBufferObject& operator=(PixelBufferObject&& other) noexcept;

    // Create a PBO with specified size and usage (GL_STREAM_READ/GL_STREAM_DRAW, etc.)
    void Create(GLsizeiptr size, GLenum usage = GL_STREAM_READ);

    // Bind or unbind the PBO to a given target (default: GL_PIXEL_PACK_BUFFER)
    void Bind(GLenum target = GL_PIXEL_PACK_BUFFER) const;
    void Unbind(GLenum target = GL_PIXEL_PACK_BUFFER) const;

    // Map buffer memory for CPU access (returns pointer or nullptr on failure)
    void* Map(GLenum access = GL_READ_ONLY);
    void Unmap();

    // Convenience: read a single uint32_t value from mapped data
    uint32_t ReadUInt32();

    // Delete GPU buffer manually
    void Destroy();

    inline GLuint ID() const { return m_id; }
    inline GLsizeiptr Size() const { return m_size; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_size = 0;
    GLenum m_usage = GL_STREAM_READ;
    void* m_mappedPtr = nullptr;
};
