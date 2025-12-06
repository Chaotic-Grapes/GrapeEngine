/* Start Header *****************************************************************/
/*!
\file   GLFWPlatformContext.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GLFW-based platform context.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "platform/glfw/GLFWPlatformContext.h"
#include "core/Logger.h"

namespace Platform {

    GLFWPlatformContext::GLFWPlatformContext() = default;

    GLFWPlatformContext::~GLFWPlatformContext() {
        Shutdown();
    }

    void GLFWPlatformContext::_glfwErrorCallback(int error, const char* description) {
        LOG_ERROR("GLFW Error [" << error << "]: " << description);
    }

    bool GLFWPlatformContext::Initialize() {
        if (m_initialized) {
            LOG_WARNING("GLFWPlatformContext already initialized");
            return true;
        }

        LOG_INFO("Initializing GLFW Platform Context...");

        // Set error callback
        glfwSetErrorCallback(_glfwErrorCallback);

        // Initialize GLFW
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            return false;
        }

        LOG_INFO("GLFW initialized successfully");
        LOG_INFO("GLFW Version: " << glfwGetVersionString());

        // Create input system
        m_inputSystem = std::make_unique<GLFWInputSystem>();

        // Create render device (will be initialized when first window is created)
        m_renderDevice = std::make_unique<OpenGLRenderDevice>();

        m_initialized = true;
        return true;
    }

    void GLFWPlatformContext::Shutdown() {
        if (!m_initialized) return;

        LOG_INFO("Shutting down GLFW Platform Context...");

        // Destroy all windows
        for (auto* window : m_windows) {
            if (auto* glfwWindow = dynamic_cast<GLFWWindow*>(window)) {
                glfwWindow->Destroy();
            }
            delete window;
        }
        m_windows.clear();

        // Cleanup subsystems
        m_renderDevice.reset();
        m_inputSystem.reset();

        // Terminate GLFW
        glfwTerminate();

        m_initialized = false;
        LOG_INFO("GLFW Platform Context shutdown complete");
    }

    bool GLFWPlatformContext::IsInitialized() const {
        return m_initialized;
    }

    IWindow* GLFWPlatformContext::CreatePlatformWindow(const WindowCreateInfo& info) {
        if (!m_initialized) {
            LOG_ERROR("Cannot create window: platform not initialized");
            return nullptr;
        }

        // Map platform-agnostic WindowMode to implementation
        WindowMode mode = info.Mode;

        // Create GLFW window
        auto* window = GLFWWindow::Create(info.Title, info.Width, info.Height, 
                                          info.VSync, mode);
        if (!window) {
            LOG_ERROR("Failed to create GLFW window");
            return nullptr;
        }

        // Initialize render device with first window
        if (m_windows.empty() && m_renderDevice) {
            LOG_INFO("Initializing render device with GLFWwindow*=" << reinterpret_cast<void*>(window->GetGLFWHandle()));
            if (!m_renderDevice->Initialize(window->GetGLFWHandle())) {
                LOG_ERROR("Render device initialization failed for window: " << info.Title);
            }
        }

        // Initialize input system with first window
        if (m_windows.empty() && m_inputSystem) {
            LOG_INFO("Initializing input system with GLFWwindow*=" << reinterpret_cast<void*>(window->GetGLFWHandle()));
            m_inputSystem->Initialize(window->GetGLFWHandle());
        }

        m_windows.push_back(window);
        LOG_INFO("Created window: " << info.Title << " (" << info.Width << "x" << info.Height << ")");

        return window;
    }

    void GLFWPlatformContext::DestroyWindow(IWindow* window) {
        if (!window) return;

        auto it = std::find(m_windows.begin(), m_windows.end(), window);
        if (it != m_windows.end()) {
            if (auto* glfwWindow = dynamic_cast<GLFWWindow*>(window)) {
                glfwWindow->Destroy();
            }
            delete window;
            m_windows.erase(it);
            LOG_INFO("Destroyed window");
        }
    }

    IWindow* GLFWPlatformContext::GetMainWindow() const {
        return m_windows.empty() ? nullptr : m_windows[0];
    }

    const std::vector<IWindow*>& GLFWPlatformContext::GetAllWindows() const {
        return m_windows;
    }

    IRenderDevice* GLFWPlatformContext::GetRenderDevice() const {
        return m_renderDevice.get();
    }

    IInputSystem* GLFWPlatformContext::GetInputSystem() const {
        return m_inputSystem.get();
    }

    std::string GLFWPlatformContext::GetPlatformName() const {
        return "GLFW";
    }

    std::string GLFWPlatformContext::GetPlatformVersion() const {
        return glfwGetVersionString();
    }

}
