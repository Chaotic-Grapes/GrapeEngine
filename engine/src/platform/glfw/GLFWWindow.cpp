/* Start Header *****************************************************************/
/*!
\file   GLFWWindow.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GLFW-backed window abstraction.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "platform/glfw/GLFWWindow.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "services/Input.h"
#include "services/DeviceManager.h"

namespace Platform {

    GLFWWindow::~GLFWWindow() {
        Destroy();
    }

    GLFWWindow* GLFWWindow::Create(const std::string& title, int width, int height,
                                    bool vsync, WindowMode mode) {
        auto* window = new GLFWWindow();
        if (window->Initialize(title, width, height, vsync, mode)) {
            return window;
        }
        delete window;
        return nullptr;
    }

    bool GLFWWindow::Initialize(const std::string& title, int width, int height,
                                bool vsync, WindowMode mode) {
        // Configure GLFW hints
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Set decorations based on mode
        if (HasFlag(mode, WindowMode::Borderless)) {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }
        else {
            glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        }

        // Create window
        GLFWmonitor* monitor = nullptr;
        if (HasFlag(mode, WindowMode::Fullscreen)) {
            monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
            if (videoMode) {
                width = videoMode->width;
                height = videoMode->height;
            }
        }

        m_windowHandle = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
        if (!m_windowHandle) {
            LOG_ERROR("Failed to create GLFW window");
            return false;
        }

        m_title = title;
        m_width = width;
        m_height = height;
        m_vsync = vsync;

        glfwMakeContextCurrent(m_windowHandle);

        // Initialize GLAD
        if (!gladLoadGL()) {
            LOG_ERROR("Failed to initialize GLAD");
            glfwDestroyWindow(m_windowHandle);
            m_windowHandle = nullptr;
            return false;
        }

        // Set VSync
        glfwSwapInterval(vsync ? 1 : 0);

        // Initialize Input System
        Input::Initialize(m_windowHandle);
        Input::SetupEventCallbacks();

        // Set viewport
        glViewport(0, 0, width, height);

        // Setup callbacks
        _setupCallbacks();

        LOG_INFO("[GLFWWindow] Created successfully: " << width << "x" << height);
        return true;
    }

    void GLFWWindow::Destroy() {
        if (m_windowHandle) {
            glfwDestroyWindow(m_windowHandle);
            m_windowHandle = nullptr;
        }
    }

    void GLFWWindow::_setupCallbacks() {
        if (!m_windowHandle) return;

        glfwSetWindowUserPointer(m_windowHandle, this);

        glfwSetFramebufferSizeCallback(m_windowHandle, [](GLFWwindow* w, int fbw, int fbh) {
            glViewport(0, 0, fbw, fbh);
            if (auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w))) {
                self->m_width = fbw;
                self->m_height = fbh;
            }
            Messaging::MessageSystem::Broadcast(Messaging::WindowResized{ fbw, fbh });
        });

        glfwSetWindowFocusCallback(m_windowHandle, [](GLFWwindow* w, int focused) {
            (void)w;
            Messaging::MessageSystem::Broadcast(Messaging::WindowFocusChanged{ focused != 0 });
        });
    }

    // ==================== IWindow Implementation ====================

    void GLFWWindow::PollEvents() const {
        glfwPollEvents();
    }

    void GLFWWindow::SwapBuffers() const {
        if (m_windowHandle) {
            glfwSwapBuffers(m_windowHandle);
        }
    }

    bool GLFWWindow::ShouldClose() const {
        return m_windowHandle && glfwWindowShouldClose(m_windowHandle);
    }

    void GLFWWindow::Close() const {
        if (m_windowHandle) {
            glfwSetWindowShouldClose(m_windowHandle, true);
        }
    }

    int GLFWWindow::GetWidth() const {
        return m_width;
    }

    int GLFWWindow::GetHeight() const {
        return m_height;
    }

    std::string GLFWWindow::GetTitle() const {
        return m_title;
    }

    void GLFWWindow::SetTitle(const std::string& title) {
        m_title = title;
        if (m_windowHandle) {
            glfwSetWindowTitle(m_windowHandle, title.c_str());
        }
    }

    bool GLFWWindow::IsFocused() const {
        return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_FOCUSED);
    }

    bool GLFWWindow::IsMinimized() const {
        return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_ICONIFIED);
    }

    void GLFWWindow::SetMinimized(bool minimized) const {
        if (!m_windowHandle) return;
        if (minimized) {
            glfwIconifyWindow(m_windowHandle);
        }
        else {
            glfwRestoreWindow(m_windowHandle);
        }
    }

    bool GLFWWindow::IsMaximized() const {
        return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_MAXIMIZED);
    }

    void GLFWWindow::SetMaximized(bool maximized) const {
        if (!m_windowHandle)
            return;
        if (maximized) {
            glfwMaximizeWindow(m_windowHandle);
        }
        else {
            glfwRestoreWindow(m_windowHandle);
        }
    }

    bool GLFWWindow::IsVisible() const {
        return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_VISIBLE);
    }

    void GLFWWindow::SetVisible(bool visible) const {
        if (!m_windowHandle)
            return;
        if (visible) {
            glfwShowWindow(m_windowHandle);
        }
        else {
            glfwHideWindow(m_windowHandle);
        }
    }

    bool GLFWWindow::IsResizable() const {
        return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_RESIZABLE);
    }

    void GLFWWindow::SetResizable(bool resizable) const {
        if (m_windowHandle) {
            glfwSetWindowAttrib(m_windowHandle, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    bool GLFWWindow::IsVSync() const { return m_vsync; }

    void GLFWWindow::SetVSync(bool enabled) {
        m_vsync = enabled;
        glfwSwapInterval(enabled ? 1 : 0);
    }

    void GLFWWindow::SetMode(WindowMode mode) {
        // Note: Full mode switching not yet implemented in platform abstraction
        // This would require converting Platform::WindowMode to legacy WindowMode::Flags
        // and calling the appropriate GLFW functions
        LOG_WARNING("GLFWWindow::SetMode() not yet implemented in platform abstraction");
    }

    bool GLFWWindow::HasMode(WindowMode mode) const {
        // Note: Mode tracking not yet implemented in platform abstraction
        LOG_WARNING("GLFWWindow::HasMode() not yet implemented in platform abstraction");
        return false;
    }

    void GLFWWindow::Resize(int width, int height) {
        if (!m_windowHandle) return;
        
        int newWidth = (width > 0) ? width : m_width;
        int newHeight = (height > 0) ? height : m_height;
        
        glfwSetWindowSize(m_windowHandle, newWidth, newHeight);
        m_width = newWidth;
        m_height = newHeight;
    }

    void* GLFWWindow::GetNativeHandle() const {
        return m_windowHandle;
    }

    // ==================== Display Mode Queries ====================

    std::vector<Engine::DisplayMode> GLFWWindow::GetSupportedDisplayModes() const {
        // Query available display modes using DeviceManager
        if (!m_windowHandle) {
            return {};
        }

        // For now, use DeviceManager to get primary monitor modes
        // In the future, could determine which monitor the window is on
        auto modes = Engine::DeviceManager::GetDisplayModes(0);
        return modes;
    }

    Engine::MonitorInfo GLFWWindow::GetMonitorInfo() const {
        if (!m_windowHandle) {
            return Engine::MonitorInfo{};
        }

        // For now, return primary monitor info
        // In the future, could determine which monitor the window is on
        auto monitors = Engine::DeviceManager::EnumerateMonitors();
        if (!monitors.empty()) {
            return monitors[0];
        }

        return Engine::MonitorInfo{};
    }

    bool GLFWWindow::SetDisplayMode(const Engine::DisplayMode& mode) {
        if (!m_windowHandle) {
            LOG_ERROR("Window handle invalid");
            return false;
        }

        // Resize window to specified mode
        glfwSetWindowSize(m_windowHandle, mode.Width, mode.Height);
        m_width = mode.Width;
        m_height = mode.Height;

        LOG_INFO("Display mode set to " << mode.ToString());
        return true;
    }

    bool GLFWWindow::SetFullscreen(bool fullscreen) {
        if (!m_windowHandle) {
            LOG_ERROR("Window handle invalid");
            return false;
        }

        // Get the monitor this window is currently on
        GLFWmonitor* monitor = glfwGetWindowMonitor(m_windowHandle);
        
        if (fullscreen && !monitor) {
            // Switch to fullscreen on primary monitor
            monitor = glfwGetPrimaryMonitor();
            if (!monitor) {
                LOG_ERROR("No monitor found for fullscreen");
                return false;
            }
            
            // Get current video mode for refresh rate
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode) {
                LOG_ERROR("Failed to get video mode");
                return false;
            }
            
            // Switch to fullscreen
            glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0, 
                                 mode->width, mode->height, mode->refreshRate);
            m_width = mode->width;
            m_height = mode->height;
            LOG_INFO("Switched to fullscreen mode: " << mode->width << "x" << mode->height 
                     << " @ " << mode->refreshRate << "Hz");
            return true;
        }
        else if (!fullscreen && monitor) {
            // Switch to windowed mode
            // Restore to a reasonable windowed size
            int restoredWidth = 1600;
            int restoredHeight = 900;
            
            // Get monitor position for window placement
            int monitorX, monitorY;
            glfwGetMonitorPos(monitor, &monitorX, &monitorY);
            
            // Center window on monitor
            int posX = monitorX + (1920 / 2) - (restoredWidth / 2);
            int posY = monitorY + (1080 / 2) - (restoredHeight / 2);
            
            glfwSetWindowMonitor(m_windowHandle, nullptr, posX, posY, 
                                 restoredWidth, restoredHeight, 0);
            m_width = restoredWidth;
            m_height = restoredHeight;

            LOG_INFO("Switched to windowed mode: " << restoredWidth << "x" << restoredHeight);
            return true;
        }
        else {
            // Already in requested mode
            if (fullscreen) {
                LOG_INFO("Already in fullscreen mode");
            }
            else {
                LOG_INFO("Already in windowed mode");
            }
            return true;
        }
    }

}
