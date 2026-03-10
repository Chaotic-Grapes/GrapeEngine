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
#include <algorithm>

namespace Platform {

    GLFWWindow::~GLFWWindow() {
        Destroy();
    }

    GLFWWindow* GLFWWindow::Create(const std::string& title, int width, int height,
                                    bool vsync, WindowMode mode, bool resizable, bool decorated) {
        auto* window = new GLFWWindow();
        if (window->Initialize(title, width, height, vsync, mode, resizable, decorated)) {
            return window;
        }
        delete window;
        return nullptr;
    }

    bool GLFWWindow::Initialize(const std::string& title, int width, int height,
                                bool vsync, WindowMode mode, bool resizable, bool decorated) {
        constexpr int kDefaultWindowWidth = 1600;
        constexpr int kDefaultWindowHeight = 900;
        constexpr int kMinWindowWidth = 640;
        constexpr int kMinWindowHeight = 360;

        const int inputWidth = width;
        const int inputHeight = height;
        width = (width <= 0) ? kDefaultWindowWidth : std::max(width, kMinWindowWidth);
        height = (height <= 0) ? kDefaultWindowHeight : std::max(height, kMinWindowHeight);
        if (inputWidth != width || inputHeight != height) {
            LOG_WARNING("[GLFWWindow] Invalid window size " << inputWidth << "x" << inputHeight
                        << ", using " << width << "x" << height);
        }

        // Configure GLFW hints
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

        const bool wantsBorderless = HasFlag(mode, WindowMode::Borderless);
        const bool wantsFullscreen = HasFlag(mode, WindowMode::Fullscreen);

        // Set decorations based on mode
        if (wantsBorderless || !decorated) {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }
        else {
            glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        }

        // Create window
        GLFWmonitor* monitor = nullptr;
        const int requestedWidth = width;
        const int requestedHeight = height;
        if (wantsFullscreen) {
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            const GLFWvidmode* videoMode = glfwGetVideoMode(primary);
            if (videoMode) {
                width = videoMode->width;
                height = videoMode->height;
            }

            if (!wantsBorderless) {
                monitor = primary;
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
        m_resizable = resizable;
        m_decorated = decorated;
        m_mode = mode;
        if (HasFlag(mode, WindowMode::Fullscreen)) {
            m_currentMonitorIndex = 0;
        }
        m_windowedWidth = requestedWidth;
        m_windowedHeight = requestedHeight;
        glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
        m_windowedValid = true;

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

        // Use framebuffer pixel size (not logical window size) for initial viewport.
        int framebufferWidth = width;
        int framebufferHeight = height;
        glfwGetFramebufferSize(m_windowHandle, &framebufferWidth, &framebufferHeight);
        m_width = framebufferWidth;
        m_height = framebufferHeight;
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // Setup callbacks
        _setupCallbacks();

        if (wantsBorderless) {
            // Borderless should cover the current monitor on startup.
            _setBorderless(true);
            m_mode = WindowMode::Borderless;
        }

        LOG_INFO("[GLFWWindow] Created successfully: "
                 << framebufferWidth << "x" << framebufferHeight
                 << " framebuffer pixels");
        return true;
    }

    void GLFWWindow::Destroy() {
        if (m_windowHandle) {
            glfwDestroyWindow(m_windowHandle);
            m_windowHandle = nullptr;
        }
    }

    GLFWmonitor* GLFWWindow::_getCurrentMonitor() const {
        if (!m_windowHandle) {
            return glfwGetPrimaryMonitor();
        }

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (!monitors || monitorCount <= 0) {
            return glfwGetPrimaryMonitor();
        }

        int winX = 0;
        int winY = 0;
        int winW = 0;
        int winH = 0;
        glfwGetWindowPos(m_windowHandle, &winX, &winY);
        glfwGetWindowSize(m_windowHandle, &winW, &winH);

        const int centerX = winX + winW / 2;
        const int centerY = winY + winH / 2;

        for (int i = 0; i < monitorCount; ++i) {
            int monX = 0;
            int monY = 0;
            glfwGetMonitorPos(monitors[i], &monX, &monY);

            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (!mode) {
                continue;
            }

            const int monW = mode->width;
            const int monH = mode->height;

            if (centerX >= monX && centerX < monX + monW &&
                centerY >= monY && centerY < monY + monH) {
                return monitors[i];
            }
        }

        return glfwGetPrimaryMonitor();
    }

    void GLFWWindow::_storeWindowedPlacement() {
        if (!m_windowHandle) {
            return;
        }

        glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
        m_windowedValid = true;
    }

    void GLFWWindow::_restoreWindowedPlacement() {
        if (!m_windowHandle) {
            return;
        }

        if (!m_windowedValid) {
            GLFWmonitor* monitor = _getCurrentMonitor();
            if (!monitor) {
                return;
            }

            int workX = 0;
            int workY = 0;
            int workW = 0;
            int workH = 0;
            glfwGetMonitorWorkarea(monitor, &workX, &workY, &workW, &workH);

            m_windowedWidth = (workW > 0) ? std::min(workW, 1600) : 1600;
            m_windowedHeight = (workH > 0) ? std::min(workH, 900) : 900;
            m_windowedX = workX + (workW - m_windowedWidth) / 2;
            m_windowedY = workY + (workH - m_windowedHeight) / 2;
            m_windowedValid = true;
        }

        glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, m_decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwSetWindowAttrib(m_windowHandle, GLFW_RESIZABLE, m_resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwSetWindowMonitor(m_windowHandle, nullptr, m_windowedX, m_windowedY,
                             m_windowedWidth, m_windowedHeight, 0);
        m_width = m_windowedWidth;
        m_height = m_windowedHeight;
        m_borderlessLockedMonitorIndex = -1;
    }

    void GLFWWindow::_lockBorderlessToMonitor() {
        if (!m_windowHandle || m_borderlessLockInProgress) {
            return;
        }

        GLFWmonitor* monitor = _getCurrentMonitor();
        if (!monitor) {
            return;
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) {
            return;
        }

        int monX = 0;
        int monY = 0;
        glfwGetMonitorPos(monitor, &monX, &monY);

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        int monitorIndex = -1;
        for (int i = 0; i < monitorCount; ++i) {
            if (monitors[i] == monitor) {
                monitorIndex = i;
                break;
            }
        }

        if (monitorIndex == m_borderlessLockedMonitorIndex) {
            // Already locked to this monitor; skip redundant snap.
            return;
        }

        int winX = 0;
        int winY = 0;
        int winW = 0;
        int winH = 0;
        glfwGetWindowPos(m_windowHandle, &winX, &winY);
        glfwGetWindowSize(m_windowHandle, &winW, &winH);

        if (winX == monX && winY == monY && winW == mode->width && winH == mode->height) {
            // Window already matches monitor bounds; avoid another resize.
            if (monitorIndex >= 0) {
                m_currentMonitorIndex = monitorIndex;
                m_borderlessLockedMonitorIndex = monitorIndex;
            }
            return;
        }

        m_borderlessLockInProgress = true;
        // Enforce borderless, monitor-sized window without switching to exclusive fullscreen.
        glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(m_windowHandle, nullptr, monX, monY,
                             mode->width, mode->height, 0);
        m_borderlessLockInProgress = false;

        if (monitorIndex >= 0) {
            m_currentMonitorIndex = monitorIndex;
            m_borderlessLockedMonitorIndex = monitorIndex;
        }
        m_width = mode->width;
        m_height = mode->height;
    }

    bool GLFWWindow::_setBorderless(bool borderless) {
        if (!m_windowHandle) {
            return false;
        }

        if (borderless) {
            if (!HasFlag(m_mode, WindowMode::Borderless)) {
                _storeWindowedPlacement();
            }
            _lockBorderlessToMonitor();
            return true;
        }

        _restoreWindowedPlacement();
        return true;
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

        glfwSetWindowPosCallback(m_windowHandle, [](GLFWwindow* w, int x, int y) {
            (void)x;
            (void)y;
            if (auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(w))) {
                if (self->HasMode(WindowMode::Borderless)) {
                    // Keep borderless windows snapped to the monitor they are on.
                    self->_lockBorderlessToMonitor();
                }
            }
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
        const_cast<GLFWWindow*>(this)->m_resizable = resizable;
    }

    bool GLFWWindow::IsVSync() const { return m_vsync; }

    void GLFWWindow::SetVSync(bool enabled) {
        m_vsync = enabled;
        glfwSwapInterval(enabled ? 1 : 0);
    }

    void GLFWWindow::SetMode(WindowMode mode) {
        if (!m_windowHandle) {
            return;
        }

        if (HasFlag(mode, WindowMode::Fullscreen)) {
            SetFullscreen(true);
            return;
        }

        if (HasFlag(mode, WindowMode::Borderless)) {
            _setBorderless(true);
            m_mode = WindowMode::Borderless;
            return;
        }

        _setBorderless(false);
        m_mode = WindowMode::Windowed;
    }

    bool GLFWWindow::HasMode(WindowMode mode) const {
        return HasFlag(m_mode, mode);
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

        if (HasFlag(m_mode, WindowMode::Fullscreen) && !HasFlag(m_mode, WindowMode::Borderless)) {
            GLFWmonitor* monitor = glfwGetWindowMonitor(m_windowHandle);
            if (!monitor) {
                monitor = _getCurrentMonitor();
            }

            glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0,
                                 mode.Width, mode.Height, mode.RefreshRate);
            m_width = mode.Width;
            m_height = mode.Height;
        } else {
            glfwSetWindowSize(m_windowHandle, mode.Width, mode.Height);
            m_width = mode.Width;
            m_height = mode.Height;
            m_windowedWidth = mode.Width;
            m_windowedHeight = mode.Height;
            m_windowedValid = true;
        }

        LOG_INFO("Display mode set to " << mode.ToString());
        return true;
    }

    bool GLFWWindow::SetFullscreen(bool fullscreen) {
        return SetFullscreenOnMonitor(fullscreen ? m_currentMonitorIndex : -1);
    }

    bool GLFWWindow::SetFullscreenOnMonitor(int monitorIndex) {
        if (!m_windowHandle) {
            LOG_ERROR("Window handle invalid");
            return false;
        }

        if (monitorIndex < 0) {
            if (!HasFlag(m_mode, WindowMode::Fullscreen)) {
                LOG_INFO("Already in windowed mode");
                return true;
            }

            _restoreWindowedPlacement();
            m_mode = WindowMode::Windowed;
            LOG_INFO("Switched to windowed mode: " << m_width << "x" << m_height);
            return true;
        }

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (!monitors || monitorCount <= 0) {
            LOG_ERROR("No monitors found for fullscreen");
            return false;
        }

        if (monitorIndex >= monitorCount) {
            LOG_ERROR("Invalid monitor index for fullscreen: " << monitorIndex);
            return false;
        }

        GLFWmonitor* monitor = monitors[monitorIndex];
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) {
            LOG_ERROR("Failed to get video mode");
            return false;
        }

        if (!HasFlag(m_mode, WindowMode::Fullscreen)) {
            _storeWindowedPlacement();
        }

        glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        m_width = mode->width;
        m_height = mode->height;
        m_mode = WindowMode::Fullscreen;
        m_currentMonitorIndex = monitorIndex;

        LOG_INFO("Switched to fullscreen mode: " << mode->width << "x" << mode->height
                 << " @ " << mode->refreshRate << "Hz");
        return true;
    }

}
