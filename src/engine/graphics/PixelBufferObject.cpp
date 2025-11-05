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
        Destroy();

        m_id = other.m_id;
        m_size = other.m_size;
        m_usage = other.m_usage;
        m_mappedPtr = nullptr;

        other.m_id = 0;
        other.m_size = 0;
    }
    return *this;
}

// ============================================================================
// Creates the PBO with specified size and usage pattern.
// ============================================================================
void PixelBufferObject::Create(GLsizeiptr size, GLenum usage)
{
    Destroy(); // delete existing buffer if any
    m_size = size;
    m_usage = usage;

    glGenBuffers(1, &m_id);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_id);
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
// Maps the buffer into client memory for reading or writing.
// ============================================================================
void* PixelBufferObject::Map(GLenum access)
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_id);
    m_mappedPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, access);
    if (!m_mappedPtr)
        std::cerr << "Failed to map PBO!" << std::endl;
    return m_mappedPtr;
}

// ============================================================================
// Unmaps the PBO from client memory after access.
// ============================================================================
void PixelBufferObject::Unmap()
{
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    m_mappedPtr = nullptr;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

// ============================================================================
// Convenience: reads a single uint32_t from mapped buffer.
// (Assumes glReadPixels already wrote data to this buffer.)
// ============================================================================
uint32_t PixelBufferObject::ReadUInt32()
{
    uint32_t value = 0;
    void* ptr = Map(GL_READ_ONLY);
    if (ptr)
    {
        value = *reinterpret_cast<uint32_t*>(ptr);
        Unmap();
    }
    return value;
}

// ============================================================================
// Deletes the GPU buffer if it exists.
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
