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

This class is primarily used in the engine�s object picking system, enabling
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

    /**
     * @brief Move constructor; transfers buffer ownership without reallocating GPU memory.
     * @param other Source PBO to move from (left in a null state).
     */
    PixelBufferObject(PixelBufferObject&& other) noexcept;

    /**
     * @brief Move assignment; transfers buffer ownership without reallocating GPU memory.
     * @param other Source PBO to move from (left in a null state).
     * @return Reference to this PBO after assignment.
     */
    PixelBufferObject& operator=(PixelBufferObject&& other) noexcept;

    /**
     * @brief Create a PBO and allocate GPU memory with the given size and usage hint.
     * @param size Buffer size in bytes.
     * @param usage OpenGL usage hint (e.g. GL_STREAM_READ or GL_STREAM_DRAW).
     */
    void Create(GLsizeiptr size, GLenum usage = GL_STREAM_READ);

    /**
     * @brief Bind the PBO to the given target.
     * @param target OpenGL PBO binding target (default: GL_PIXEL_PACK_BUFFER).
     */
    void Bind(GLenum target = GL_PIXEL_PACK_BUFFER) const;

    /**
     * @brief Unbind any PBO from the given target.
     * @param target OpenGL PBO binding target (default: GL_PIXEL_PACK_BUFFER).
     */
    void Unbind(GLenum target = GL_PIXEL_PACK_BUFFER) const;

    /**
     * @brief Map the buffer memory for CPU read/write access.
     * @param access Mapping access mode (e.g. GL_READ_ONLY, GL_WRITE_ONLY).
     * @return CPU-visible pointer to the mapped buffer, or nullptr on failure.
     */
    void* Map(GLenum access = GL_READ_ONLY);

    /**
     * @brief Unmap the previously mapped buffer memory.
     */
    void Unmap();

    /**
     * @brief Read a single uint32_t value from the mapped buffer data.
     * @return The uint32_t value at the start of the mapped region.
     */
    uint32_t ReadUInt32();

    /**
     * @brief Delete the GPU buffer and reset all state.
     */
    void Destroy();

    /**
     * @brief Return the underlying OpenGL buffer object handle.
     * @return OpenGL buffer object ID.
     */
    inline GLuint ID() const { return m_id; }

    /**
     * @brief Return the allocated buffer size in bytes.
     * @return Buffer size in bytes.
     */
    inline GLsizeiptr Size() const { return m_size; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_size = 0;
    GLenum m_usage = GL_STREAM_READ;
    void* m_mappedPtr = nullptr;
};
