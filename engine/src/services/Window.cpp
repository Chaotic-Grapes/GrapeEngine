/* Start Header *****************************************************************/
/*!
\file   Window.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Implements the Window service which manages the application window using GLFW.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/Window.h"
#include "services/Input.h"
#include <iostream>
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

namespace {
	bool HasFlag(const WindowMode::Flags a, const WindowMode::Flags b) {
		return (a & b) != static_cast<WindowMode::Flags>(0);
	}
}

//static void FramebufferSizeCallback(GLFWwindow* window, const int width, const int height) {
//	(void)window;
//	glViewport(0, 0, width, height);
//
//	// Broadcast resize message
//	Messaging::MessageSystem::Broadcast(Messaging::WindowResized{ width, height });
//}

Window::~Window() { Destroy(); }

bool Window::Create(const std::string& title, int width, int height, 
                    bool vsync, WindowMode::Flags mode, 
                    GLFWmonitor* monitor, GLFWwindow* parent) {
    
    // Validate mode combination
    if (HasFlag(mode, WindowMode::Borderless) && HasFlag(mode, WindowMode::Fullscreen)) {
        LOG_ERROR("Invalid mode combination: Borderless and Fullscreen cannot be used together");
        return false;
    }

    // Default to windowed if no mode specified
    if (mode == static_cast<WindowMode::Flags>(0)) {
        mode = WindowMode::Windowed;
        LOG_WARNING("No window mode specified, defaulting to Windowed");
    }

    // Default to primary monitor if none specified
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();

    const GLFWvidmode* vm = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!vm) {
        LOG_ERROR("Failed to query video mode for monitor");
        return false;
    }

    // Determine creation size based on mode
    int createWidth = width;
    int createHeight = height;

    // If invalid size provided, use monitor resolution
    if (createWidth <= 0 || createHeight <= 0) {
        createWidth = vm->width;
        createHeight = vm->height;
        LOG_WARNING("Invalid window size provided (" << width << "x" << height 
                    << "), using monitor resolution: " << createWidth << "x" << createHeight);
    }

    // For fullscreen/borderless modes, ALWAYS use monitor resolution
    if (HasFlag(mode, WindowMode::Fullscreen) || HasFlag(mode, WindowMode::Borderless)) {
        createWidth = vm->width;
        createHeight = vm->height;
        LOG_INFO("Using monitor resolution for " 
                 << (HasFlag(mode, WindowMode::Fullscreen) ? "fullscreen" : "borderless") 
                 << " mode: " << createWidth << "x" << createHeight);
    }

    // Store dimensions
    m_width = createWidth;
    m_height = createHeight;

    // Configure GLFW hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Set decorations based on mode (for initial creation)
    if (HasFlag(mode, WindowMode::Borderless)) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    } 
	else {
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }

    // Create the window
    // For exclusive fullscreen, attach to monitor during creation
    GLFWmonitor* createMonitor = HasFlag(mode, WindowMode::Fullscreen) ? monitor : nullptr;
    
    m_windowHandle = glfwCreateWindow(createWidth, createHeight, title.c_str(), 
                                     createMonitor, parent);
    m_title = title;

    if (!m_windowHandle) {
        LOG_ERROR("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(m_windowHandle);

    // Initialize OpenGL
    if (!gladLoadGL()) {
        LOG_ERROR("Failed to initialize GLAD");
        glfwDestroyWindow(m_windowHandle);
        m_windowHandle = nullptr;
        return false;
    }

    // Set VSync
    SetVSync(vsync);

    // Initialize Input System
    Input::Initialize(m_windowHandle);
    Input::SetupEventCallbacks();

    // Set initial viewport
    glViewport(0, 0, m_width, m_height);

    // Store window pointer for callbacks
    glfwSetWindowUserPointer(m_windowHandle, this);

    // Setup Callbacks
    glfwSetFramebufferSizeCallback(m_windowHandle, [](GLFWwindow* w, int fbw, int fbh) {
        glViewport(0, 0, fbw, fbh);
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->m_width = fbw;
            self->m_height = fbh;
        }
        Messaging::MessageSystem::Broadcast(Messaging::WindowResized{ fbw, fbh });
    });

    glfwSetWindowFocusCallback(m_windowHandle, [](GLFWwindow* w, int focused) {
        (void)w;
        Messaging::MessageSystem::Broadcast(Messaging::WindowFocusChanged{ focused != 0 });
    });

    // Initialize windowed state for future mode switches
    if (HasFlag(mode, WindowMode::Windowed)) {
        // For windowed mode, store the initial position and size
        glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
        m_windowedWidth = createWidth;
        m_windowedHeight = createHeight;
    } 
	else {
        // For fullscreen/borderless, use user-provided size as windowed fallback
        // (or defaults if size was invalid)
        m_windowedWidth = (width > 0) ? width : 1600;
        m_windowedHeight = (height > 0) ? height : 900;
        m_windowedX = 100;
        m_windowedY = 100;
        LOG_INFO("Initialized windowed fallback state: " << m_windowedWidth 
                 << "x" << m_windowedHeight << " at (" << m_windowedX << ", " << m_windowedY << ")");
    }

    // === Lock aspect ratio (16:9) ===
    glfwSetWindowAspectRatio(m_windowHandle, 16, 9);

    // Apply mode-specific settings
    // For borderless, position it over the monitor
    if (HasFlag(mode, WindowMode::Borderless)) {
        int monX = 0, monY = 0;
        glfwGetMonitorPos(monitor, &monX, &monY);
        glfwSetWindowPos(m_windowHandle, monX, monY);
        LOG_INFO("Positioned borderless window at monitor coordinates (" 
                 << monX << ", " << monY << ")");
    }

    // Store the initial mode
    m_mode = mode;

    LOG_INFO("Window created successfully in mode: " << static_cast<int>(mode));
    return true;
}

void Window::Destroy() {
	if (m_windowHandle) {
		glfwDestroyWindow(m_windowHandle);
		m_windowHandle = nullptr;
	}
}

void Window::SetMode(const WindowMode::Flags mode, GLFWmonitor* monitor) {
    // Validate mode - Borderless + Fullscreen is invalid
    if (HasFlag(mode, WindowMode::Borderless) && HasFlag(mode, WindowMode::Fullscreen)) {
        LOG_ERROR("Invalid mode combination: Borderless and Fullscreen cannot be used together");
        return;
    }

    // No change needed
    if (mode == m_mode) return;

    LOG_INFO("Switching window mode from " << static_cast<int>(m_mode) 
             << " to " << static_cast<int>(mode));

    // Default to primary monitor if none specified
    if (!monitor) 
        monitor = glfwGetPrimaryMonitor();
    
    const GLFWvidmode* modeInfo = glfwGetVideoMode(monitor);
    if (!modeInfo) {
        LOG_ERROR("Failed to query video mode for monitor");
        return;
    }

    // Save current windowed state before transitioning away from windowed mode
    // Only save if currently in true windowed mode (not fullscreen or borderless)
    if (HasFlag(m_mode, WindowMode::Windowed) &&
		!HasFlag(m_mode, WindowMode::Fullscreen) &&
        !HasFlag(m_mode, WindowMode::Borderless)) {
        glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
        LOG_DEBUG("Saved windowed state: pos(" << m_windowedX << ", " << m_windowedY 
                  << "), size(" << m_windowedWidth << "x" << m_windowedHeight << ")");
    }

    // Exclusive Fullscreen (monitor attached, decorated doesn't matter)
    if (HasFlag(mode, WindowMode::Fullscreen)) {
        glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0, 
                           modeInfo->width, modeInfo->height, modeInfo->refreshRate);
        LOG_INFO("Switched to exclusive fullscreen: " << modeInfo->width 
                 << "x" << modeInfo->height << " @ " << modeInfo->refreshRate << "Hz");
        m_mode = mode;
        return;
    }

    // Borderless Windowed Fullscreen (no monitor, no decorations, covers screen)
    if (HasFlag(mode, WindowMode::Borderless)) {
        glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_FALSE);
        
        int monX = 0, monY = 0;
        glfwGetMonitorPos(monitor, &monX, &monY);
        
        glfwSetWindowMonitor(m_windowHandle, nullptr, monX, monY, 
                           modeInfo->width, modeInfo->height, GLFW_DONT_CARE);
        LOG_INFO("Switched to borderless windowed: " << modeInfo->width 
                 << "x" << modeInfo->height << " at monitor pos(" << monX << ", " << monY << ")");
        m_mode = mode;
        return;
    }

    // Regular Windowed (no monitor, with decorations, custom size/pos)
    if (HasFlag(mode, WindowMode::Windowed)) {
        glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_TRUE);
        
        // Validate stored dimensions before restoring
        if (m_windowedWidth <= 0 || m_windowedHeight <= 0) {
            LOG_WARNING("Invalid stored windowed dimensions, using defaults");
            m_windowedWidth = 1600;
            m_windowedHeight = 900;
            m_windowedX = 100;
            m_windowedY = 100;
        }
        
        glfwSetWindowMonitor(m_windowHandle, nullptr, 
                           m_windowedX, m_windowedY, 
                           m_windowedWidth, m_windowedHeight, GLFW_DONT_CARE);
        LOG_INFO("Switched to windowed: " << m_windowedWidth << "x" << m_windowedHeight 
                 << " at pos(" << m_windowedX << ", " << m_windowedY << ")");
        m_mode = mode;
        return;
    }

    // No valid flags set, so treat as windowed
    LOG_WARNING("No valid window mode flags set, defaulting to windowed");
    SetMode(WindowMode::Windowed, monitor);
}

void Window::Resize(std::optional<int> width, std::optional<int> height) {
	if (HasFlag(m_mode, WindowMode::Fullscreen) || HasFlag(m_mode, WindowMode::Borderless))
		return; // Cannot resize in fullscreen or borderless mode
	
	glfwSetWindowSize(m_windowHandle, width.value_or(m_width), height.value_or(m_height));
	m_width = width.value_or(m_width);
	m_height = height.value_or(m_height);
}

void Window::SetTitle(const std::string& title) {
	m_title = title;
	if (m_windowHandle)
		glfwSetWindowTitle(m_windowHandle, title.c_str());
}

std::string Window::GetTitle() const { return m_title; }

GLFWwindow* Window::Handle()  const { return m_windowHandle; }
int Window::GetWidth()						const { return m_width; }
int Window::GetHeight()					const { return m_height; }
void Window::PollEvents()		  const { glfwPollEvents(); }
void Window::SwapBuffers()		const { glfwSwapBuffers(m_windowHandle); }
bool Window::ShouldClose()		const { return m_windowHandle && glfwWindowShouldClose(m_windowHandle); }
void Window::Close()			    const { glfwSetWindowShouldClose(m_windowHandle, true); }

bool Window::IsFocused()				  			   const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_FOCUSED); }
bool Window::IsMinimized()				  			 const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_ICONIFIED); }
void Window::SetMinimized(const bool flag) const {
	if (!m_windowHandle) return;
	if (flag)
		glfwIconifyWindow(m_windowHandle);
	else
		glfwRestoreWindow(m_windowHandle);
}
bool Window::IsMaximized() const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_MAXIMIZED); }
void Window::SetMaximized(const bool flag) const {
	if (!m_windowHandle) return;
	if (flag)
		glfwMaximizeWindow(m_windowHandle);
	else
		glfwRestoreWindow(m_windowHandle);
}
bool Window::IsVisible() const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_VISIBLE); }
void Window::SetVisible(const bool flag) const {
	if (!m_windowHandle) return;
	if (flag)
		glfwShowWindow(m_windowHandle);
	else
		glfwHideWindow(m_windowHandle);
}
bool Window::IsResizable()				  			 const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_RESIZABLE); }
void Window::SetResizable(const bool flag) const { if (m_windowHandle) glfwSetWindowAttrib(m_windowHandle, GLFW_RESIZABLE, flag ? GLFW_TRUE : GLFW_FALSE); }

void Window::SetVSync(bool enabled) { m_vsync = enabled; glfwSwapInterval(enabled ? 1 : 0); }
bool Window::IsVSync() const 			  { return m_vsync; }

bool Window::HasMode(const WindowMode::Flags value) const { return HasFlag(m_mode, value); }
