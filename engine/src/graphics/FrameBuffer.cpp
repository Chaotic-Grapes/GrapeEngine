/* Start Header *****************************************************************/
/*!
\file   Framebuffer.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   29th October 2025
\brief
Implements the Framebuffer class, which encapsulates the creation, management,
and usage of OpenGL framebuffer objects (FBOs). A framebuffer serves as an
off-screen render target that allows rendering to textures instead of directly
to the default window backbuffer.

This file provides functions for:
- Creating framebuffers with configurable color and depth attachments
- Binding/unbinding framebuffers for off-screen rendering
- Resizing and destroying framebuffer resources
- Clearing and blitting contents between framebuffers
- Binding framebuffer textures for post-processing passes

The Framebuffer class is a key component of the engine's rendering pipeline,
used for effects such as HDR rendering, bloom, tone mapping, and deferred passes.
*/
/* End Header *******************************************************************/

#include "graphics/Framebuffer.hpp"
#include <iostream>
#include <vector>

/**
 * @brief Creates a framebuffer with color and optional depth attachments.
 *
 * Sets up texture targets for rendering into (instead of the default window).
 * Parameters allow choosing size, HDR (floating-point) mode, and number of
 * color attachments.
 *
 * @param w                   Width of the framebuffer in pixels.
 * @param h                   Height of the framebuffer in pixels.
 * @param floatingPoint        If true, color attachments use GL_RGBA16F; otherwise GL_RGBA8.
 * @param withDepth            If true, a depth/stencil renderbuffer is created.
 * @param colorAttachmentCount Number of color texture attachments to create.
 */
void Framebuffer::Create(int w, int h,
    bool floatingPoint,
    bool withDepth,
    int colorAttachmentCount)
{
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // ------------------------------------------------------------
    // Color attachments
    // ------------------------------------------------------------
    colorAttachments.resize(colorAttachmentCount);
    glGenTextures(colorAttachmentCount, colorAttachments.data());

    numColorAttachments = colorAttachmentCount;

    GLenum internalFormat = floatingPoint ? GL_RGBA16F : GL_RGBA8;
    GLenum type = floatingPoint ? GL_FLOAT : GL_UNSIGNED_BYTE;

    for (int i = 0; i < colorAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, colorAttachments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            w, h, 0, GL_RGBA, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0 + i,
            GL_TEXTURE_2D,
            colorAttachments[i], 0);
    }

    // Tell OpenGL which color attachments to render to
    if (colorAttachmentCount > 0) {
        std::vector<GLenum> attachments;
        attachments.reserve(colorAttachmentCount);
        for (int i = 0; i < colorAttachmentCount; ++i)
            attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
        glDrawBuffers(colorAttachmentCount, attachments.data());
    }
    else {
        // No color attachments: explicitly set no draw/read buffers
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    // ------------------------------------------------------------
    // Optional depth/stencil attachment
    // ------------------------------------------------------------
    if (withDepth)
    {
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            depth);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer incomplete! status=0x" << std::hex << status << std::dec << "\n";
        switch (status) {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "  Reason: INCOMPLETE_ATTACHMENT\n";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "  Reason: MISSING_ATTACHMENT\n";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cerr << "  Reason: INCOMPLETE_DRAW_BUFFER\n";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cerr << "  Reason: INCOMPLETE_READ_BUFFER\n";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "  Reason: UNSUPPORTED (check formats)\n";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            std::cerr << "  Reason: INCOMPLETE_MULTISAMPLE\n";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            std::cerr << "  Reason: INCOMPLETE_LAYER_TARGETS\n";
            break;
        default:
            std::cerr << "  Reason: Unknown\n";
            break;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Binds this framebuffer so subsequent draw calls render into it.
 */
void Framebuffer::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }

/**
 * @brief Unbinds the framebuffer, restoring rendering to the default backbuffer.
 */
void Framebuffer::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

/**
 * @brief Resizes the framebuffer by destroying and recreating all attachments
 *        with the new dimensions.
 *
 * @param w            New width in pixels.
 * @param h            New height in pixels.
 * @param floatingPoint If true, color attachments use GL_RGBA16F; otherwise GL_RGBA8.
 * @param withDepth    If true, a depth/stencil renderbuffer is recreated.
 */
void Framebuffer::Resize(int w, int h,
    bool floatingPoint,
    bool withDepth)
{
    Destroy();
    Create(w, h, floatingPoint, withDepth);
}

/**
 * @brief Frees all GPU resources associated with this framebuffer,
 *        including color textures, depth renderbuffer, and the FBO itself.
 */
void Framebuffer::Destroy()
{
    if (depth)
    {
        glDeleteRenderbuffers(1, &depth);
        depth = 0;
    }

    if (!colorAttachments.empty())
    {
        glDeleteTextures((GLsizei)colorAttachments.size(), colorAttachments.data());
        colorAttachments.clear();
    }

    if (fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
}

/**
 * @brief Binds the framebuffer, sets the viewport to its dimensions, and
 *        clears both the color and depth buffers to a specified RGBA color.
 *
 * @param r Red component of the clear color (0.0 - 1.0).
 * @param g Green component of the clear color (0.0 - 1.0).
 * @param b Blue component of the clear color (0.0 - 1.0).
 * @param a Alpha component of the clear color (0.0 - 1.0).
 */
void Framebuffer::BindAndClear(float r, float g, float b, float a)
{
    Bind();
    glViewport(0, 0, width, height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * @brief Copies (blits) the framebuffer's contents to the default backbuffer.
 *        Typically used to display post-processed results to the screen.
 *
 * @param mask   Bitfield of buffers to copy (e.g., GL_COLOR_BUFFER_BIT).
 * @param filter Interpolation filter to use if sizes differ (e.g., GL_NEAREST).
 */
void Framebuffer::BlitToDefault(GLbitfield mask, GLenum filter) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height,
        0, 0, width, height,
        mask, filter);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

/**
 * @brief Binds one of the framebuffer's color attachment textures to a
 *        specific texture unit so it can be sampled in a shader.
 *
 * @param index Zero-based index of the color attachment to bind.
 * @param unit  OpenGL texture unit index (e.g., 0 maps to GL_TEXTURE0).
 */
void Framebuffer::BindColorTexture(int index, int unit) const
{
    if (index < 0 || index >= static_cast<int>(colorAttachments.size()))
        return;

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, colorAttachments[index]);
}
