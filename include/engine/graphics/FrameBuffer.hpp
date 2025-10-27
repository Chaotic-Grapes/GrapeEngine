/* Start Header *****************************************************************/
/*!
\file   Framebuffer.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   23rd October 2025
\brief
Defines the Framebuffer class, which wraps an OpenGL framebuffer object (FBO)
used for off-screen rendering and post-processing.

This class provides a simple interface to:
- Create and manage a framebuffer with optional floating-point color precision
  and depth-stencil attachments.
- Bind and unbind framebuffers during rendering passes.
- Resize or destroy existing framebuffers safely.
- Retrieve the color texture for use in shaders or screen-space effects.

The framebuffer supports both standard 8-bit color and 16-bit floating-point
render targets, making it suitable for HDR pipelines and general render passes.
*/
/* End Header *******************************************************************/

#pragma once
#include <glad/glad.h>
#include <vector>

/**
 * @class Framebuffer
 * @brief Encapsulates an OpenGL framebuffer and its attachments.
 *
 * Provides functionality for creating, resizing, and managing off-screen
 * framebuffers used in rendering pipelines.
 */
class Framebuffer {
public:
    GLuint fbo = 0;    //!< OpenGL framebuffer object ID
    std::vector<GLuint> colorAttachments;
    int numColorAttachments = 1;
    GLuint depth = 0;  //!< Renderbuffer ID for depth-stencil attachment
    int width = 0;     //!< Framebuffer width in pixels
    int height = 0;    //!< Framebuffer height in pixels

    /**
    * @brief Default constructor. Does not create any OpenGL resources.
     */
    Framebuffer() = default;

    /**
     * @brief Destructor. Automatically destroys any allocated OpenGL objects.
     */
    ~Framebuffer() { Destroy(); }

    /**
     * @brief Creates a framebuffer with a color texture and optional depth buffer.
     * @param w Width of the framebuffer in pixels.
     * @param h Height of the framebuffer in pixels.
     * @param floatingPoint If true, creates a 16-bit floating-point color buffer.
     * @param withDepth If true, creates a depth-stencil renderbuffer attachment.
     */
    void Create(int w, int h,
        bool floatingPoint = false,
        bool withDepth = true,
        int colorAttachmentCount = 1);

    /**
     * @brief Binds this framebuffer for rendering.
     */
    void Bind();

    /**
     * @brief Unbinds any active framebuffer (binds the default screen framebuffer).
     */
    static void Unbind();

    /**
     * @brief Recreates the framebuffer with new dimensions or format.
     *        Old attachments are safely deleted.
     * @param w New width.
     * @param h New height.
     * @param floatingPoint Use floating-point color buffer if true.
     * @param withDepth Include depth-stencil attachment if true.
     */
    void Resize(int w, int h,
        bool floatingPoint = false,
        bool withDepth = true);

    /**
     * @brief Destroys all OpenGL resources associated with this framebuffer.
     */
    void Destroy();

    /**
     * @brief Returns the color texture ID for a specific attachment.
     * @param index Attachment index (default 0).
     * @return GLuint handle to the color texture.
     */
    GLuint GetColorTexture(int index = 0) const {
        return (index >= 0 && index < (int)colorAttachments.size())
            ? colorAttachments[index]
            : 0;
    }

    GLuint GetID() const { return fbo; }           // if you haven't already
    int     Width() const { return width; }
    int     Height() const { return height; }

    // CONVENIENCE HELPERS

    /** @brief Binds and clears the framebuffer in one call. */
    void BindAndClear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);

    /** @brief Binds the default framebuffer (screen). */
    static void BindDefault() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

    /**
     * @brief Blits this framebuffer to the default (screen) framebuffer.
     * @param mask  Bitmask of buffers to copy (e.g. GL_COLOR_BUFFER_BIT).
     * @param filter  Filtering mode (GL_NEAREST or GL_LINEAR).
     */
    void BlitToDefault(GLbitfield mask = GL_COLOR_BUFFER_BIT,
        GLenum filter = GL_NEAREST) const;
};
