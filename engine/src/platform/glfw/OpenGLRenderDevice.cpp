/* Start Header *****************************************************************/
/*!
\file   OpenGLRenderDevice.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of OpenGL render device abstraction.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "platform/glfw/OpenGLRenderDevice.h"
#include "core/Logger.h"

namespace Platform {

    OpenGLRenderDevice::OpenGLRenderDevice() = default;

    /**
     * @brief Initialize the render device by querying OpenGL strings and the current viewport.
     * @param window The GLFW window whose OpenGL context to use.
     * @return True on success; false if window is null.
     */
    bool OpenGLRenderDevice::Initialize(GLFWwindow* window) {
        m_window = window;

        if (!window) {
            LOG_ERROR("OpenGLRenderDevice: Cannot initialize with null window");
            return false;
        }

        // Query OpenGL info
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

        m_vendor = vendor ? vendor : "Unknown";
        m_renderer = renderer ? renderer : "Unknown";
        m_version = version ? version : "Unknown";

        LOG_INFO("OpenGL Render Device Initialized:");
        LOG_INFO("  Vendor: " << m_vendor);
        LOG_INFO("  Renderer: " << m_renderer);
        LOG_INFO("  Version: " << m_version);

        // Get initial viewport
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        m_viewportWidth = viewport[2];
        m_viewportHeight = viewport[3];

        return true;
    }

    // ==================== IRenderDevice Implementation ====================

    /**
     * @brief Return the graphics API this device implements.
     * @return GraphicsAPI::OpenGL.
     */
    GraphicsAPI OpenGLRenderDevice::GetAPI() const {
        return GraphicsAPI::OpenGL;
    }

    /**
     * @brief Return the GPU vendor string reported by OpenGL.
     * @return Vendor name string.
     */
    std::string OpenGLRenderDevice::GetVendor() const {
        return m_vendor;
    }

    /**
     * @brief Return the renderer string reported by OpenGL.
     * @return Renderer name string.
     */
    std::string OpenGLRenderDevice::GetRenderer() const {
        return m_renderer;
    }

    /**
     * @brief Return the OpenGL version string.
     * @return Version string.
     */
    std::string OpenGLRenderDevice::GetVersion() const {
        return m_version;
    }

    /**
     * @brief Make this device's OpenGL context current on the calling thread.
     */
    void OpenGLRenderDevice::MakeCurrent() {
        if (m_window) {
            glfwMakeContextCurrent(m_window);
        }
    }

    /**
     * @brief Check whether this device's context is current on the calling thread.
     * @return True if the context is active.
     */
    bool OpenGLRenderDevice::IsCurrent() const {
        return m_window && (glfwGetCurrentContext() == m_window);
    }

    /**
     * @brief Clear the color, depth, and stencil buffers.
     */
    void OpenGLRenderDevice::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    /**
     * @brief Set the RGBA color used when clearing the color buffer.
     * @param r Red component [0, 1].
     * @param g Green component [0, 1].
     * @param b Blue component [0, 1].
     * @param a Alpha component [0, 1].
     */
    void OpenGLRenderDevice::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    /**
     * @brief Set the OpenGL viewport rectangle and update cached dimensions.
     * @param x Viewport left edge in pixels.
     * @param y Viewport bottom edge in pixels.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    void OpenGLRenderDevice::SetViewport(int x, int y, int width, int height) {
        glViewport(x, y, width, height);
        m_viewportWidth = width;
        m_viewportHeight = height;
    }

    /**
     * @brief Return the cached viewport width.
     * @return Viewport width in pixels.
     */
    int OpenGLRenderDevice::GetViewportWidth() const {
        return m_viewportWidth;
    }

    /**
     * @brief Return the cached viewport height.
     * @return Viewport height in pixels.
     */
    int OpenGLRenderDevice::GetViewportHeight() const {
        return m_viewportHeight;
    }

    /**
     * @brief Enable or disable wireframe polygon rendering.
     * @param enabled True to draw wireframes; false for filled polygons.
     */
    void OpenGLRenderDevice::SetWireframeMode(bool enabled) {
        m_wireframeMode = enabled;
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }

    /**
     * @brief Check whether wireframe mode is currently active.
     * @return True if wireframe rendering is enabled.
     */
    bool OpenGLRenderDevice::IsWireframeMode() const {
        return m_wireframeMode;
    }

}
