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

    GraphicsAPI OpenGLRenderDevice::GetAPI() const {
        return GraphicsAPI::OpenGL;
    }

    std::string OpenGLRenderDevice::GetVendor() const {
        return m_vendor;
    }

    std::string OpenGLRenderDevice::GetRenderer() const {
        return m_renderer;
    }

    std::string OpenGLRenderDevice::GetVersion() const {
        return m_version;
    }

    void OpenGLRenderDevice::MakeCurrent() {
        if (m_window) {
            glfwMakeContextCurrent(m_window);
        }
    }

    bool OpenGLRenderDevice::IsCurrent() const {
        return m_window && (glfwGetCurrentContext() == m_window);
    }

    void OpenGLRenderDevice::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void OpenGLRenderDevice::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void OpenGLRenderDevice::SetViewport(int x, int y, int width, int height) {
        glViewport(x, y, width, height);
        m_viewportWidth = width;
        m_viewportHeight = height;
    }

    int OpenGLRenderDevice::GetViewportWidth() const {
        return m_viewportWidth;
    }

    int OpenGLRenderDevice::GetViewportHeight() const {
        return m_viewportHeight;
    }

    void OpenGLRenderDevice::SetWireframeMode(bool enabled) {
        m_wireframeMode = enabled;
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }

    bool OpenGLRenderDevice::IsWireframeMode() const {
        return m_wireframeMode;
    }

}
