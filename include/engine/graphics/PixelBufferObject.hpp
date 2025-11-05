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
