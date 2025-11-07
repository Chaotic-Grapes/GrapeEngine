/* Start Header *****************************************************************/
/*!
\file   PixelBufferObject.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Implements the PixelBufferObject class, which manages OpenGL Pixel Buffer
Objects (PBOs) for asynchronous pixel data transfers between the GPU and CPU.

This class encapsulates buffer creation, mapping, and cleanup, allowing
operations like object picking or screenshot capture without stalling the
rendering pipeline. By using double-buffered PBOs, the engine can read back
pixels while the GPU continues rendering subsequent frames.

Responsibilities:
- Allocate and destroy GPU-side pixel buffers
- Map and unmap buffers for CPU read access
- Support asynchronous readback via glReadPixels
- Provide safe move semantics for buffer ownership transfer
*/
/* End Header *******************************************************************/

#include "graphics/PixelBufferObject.hpp"
#include <iostream>

// ============================================================================
// Destructor: automatically deletes the OpenGL buffer.
// ============================================================================
PixelBufferObject::~PixelBufferObject()
{
    Destroy();
}

// ============================================================================
// Move constructor: transfers ownership of the OpenGL buffer.
// ============================================================================
PixelBufferObject::PixelBufferObject(PixelBufferObject&& other) noexcept
{
    m_id = other.m_id;
    m_size = other.m_size;
    m_usage = other.m_usage;
    m_mappedPtr = nullptr;

    // Reset source to avoid accidental double deletion
    other.m_id = 0;
    other.m_size = 0;
}

// ============================================================================
// Move assignment: releases current buffer, takes ownership of another.
// ============================================================================
PixelBufferObject& PixelBufferObject::operator=(PixelBufferObject&& other) noexcept
{
    if (this != &other)
    {
        Destroy();   // free any existing GPU buffer before taking ownership

        m_id = other.m_id;
        m_size = other.m_size;
        m_usage = other.m_usage;
        m_mappedPtr = nullptr;

        // Reset the moved-from object
        other.m_id = 0;
        other.m_size = 0;
    }
    return *this;
}

// ============================================================================
// Creates a PBO with the given size and usage hint (e.g., GL_STREAM_READ).
// Allocates GPU memory but does not initialize it with data.
// ============================================================================
void PixelBufferObject::Create(GLsizeiptr size, GLenum usage)
{
    Destroy(); // delete existing buffer if any to avoid leaks
    m_size = size;
    m_usage = usage;

    glGenBuffers(1, &m_id);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_id);

    // Allocate buffer storage on the GPU (nullptr = uninitialized data)
    glBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, usage);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

// ============================================================================
// Binds or unbinds the PBO to a specific OpenGL target.
// ============================================================================
void PixelBufferObject::Bind(GLenum target) const
{
    glBindBuffer(target, m_id);
}

void PixelBufferObject::Unbind(GLenum target) const
{
    glBindBuffer(target, 0);
}

// ============================================================================
// Maps the buffer into CPU memory space so it can be read or written.
// The access mode (e.g., GL_READ_ONLY) controls GPU-CPU synchronization.
// ============================================================================
void* PixelBufferObject::Map(GLenum access)
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_id);

    // Map the GPU memory into the client address space
    m_mappedPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, access);

    if (!m_mappedPtr) std::cerr << "Failed to map PBO!" << std::endl;

    return m_mappedPtr;
}

// ============================================================================
// Unmaps the PBO after CPU access is done, allowing the GPU to reuse it.
// Failing to unmap will cause synchronization issues or driver errors.
// ============================================================================
void PixelBufferObject::Unmap()
{
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    m_mappedPtr = nullptr;

    // Always unbind after unmapping to avoid accidental reuse
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

// ============================================================================
// Convenience: reads a single uint32_t from mapped buffer.
// Commonly used for object picking to retrieve encoded entity IDs.
// (Assumes glReadPixels already wrote data to this buffer.)
// ============================================================================
uint32_t PixelBufferObject::ReadUInt32()
{
    uint32_t value = 0;

    // Map GPU buffer into CPU memory for reading the result
    void* ptr = Map(GL_READ_ONLY);
    if (ptr)
    {
        value = *reinterpret_cast<uint32_t*>(ptr);
        Unmap();
    }
    return value;
}

// ============================================================================
// Deletes the GPU buffer if it exists, freeing the allocated VRAM.
// Safe to call multiple times since m_id is reset to zero after deletion.
// ============================================================================
void PixelBufferObject::Destroy()
{
    if (m_id)
    {
        glDeleteBuffers(1, &m_id);
        m_id = 0;
    }
    m_mappedPtr = nullptr;
    m_size = 0;
}
