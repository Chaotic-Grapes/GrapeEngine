#include "graphics/Framebuffer.hpp"
#include <iostream>
#include <vector>

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

    GLenum internalFormat = floatingPoint ? GL_RGBA16F : GL_RGBA8;
    GLenum type = floatingPoint ? GL_FLOAT : GL_UNSIGNED_BYTE;

    for (int i = 0; i < colorAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, colorAttachments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            w, h, 0, GL_RGBA, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0 + i,
            GL_TEXTURE_2D,
            colorAttachments[i], 0);
    }

    // Tell OpenGL which color attachments to render to
    std::vector<GLenum> attachments;
    attachments.reserve(colorAttachmentCount);
    for (int i = 0; i < colorAttachmentCount; ++i)
        attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
    glDrawBuffers(colorAttachmentCount, attachments.data());

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

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer incomplete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }
void Framebuffer::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void Framebuffer::Resize(int w, int h,
    bool floatingPoint,
    bool withDepth)
{
    Destroy();
    Create(w, h, floatingPoint, withDepth);
}

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

void Framebuffer::BindAndClear(float r, float g, float b, float a)
{
    Bind();
    glViewport(0, 0, width, height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Framebuffer::BlitToDefault(GLbitfield mask, GLenum filter) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height,
        0, 0, width, height,
        mask, filter);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}