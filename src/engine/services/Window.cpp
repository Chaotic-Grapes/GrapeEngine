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

bool Window::Create(const std::string& title, const int width, const int height, const bool vsync, const bool isFullscreen, WindowMode::Flags mode, GLFWmonitor* monitor, GLFWwindow* parent) {
	this->m_width = width;
	this->m_height = height;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	// If an invalid size was supplied (0 or negative), fall back to the monitor's resolution
	int createWidth = width;
	int createHeight = height;
	if (createWidth <= 0 || createHeight <= 0) {
		if (!monitor)
			monitor = glfwGetPrimaryMonitor();
		
		// Query monitor video mode
		const GLFWvidmode* vm = monitor ? glfwGetVideoMode(monitor) : nullptr;

		// Fallback to monitor resolution if available
		if (vm) {
			createWidth = vm->width;
			createHeight = vm->height;
			this->m_width = createWidth;
			this->m_height = createHeight;
			LOG_WARNING("Invalid window size provided, using monitor resolution: " << createWidth << "x" << createHeight);
		}
		else {
			// Critical because if monitor query fails, who knows in the long run we may run into further issues
			LOG_CRITICAL("Invalid window size and failed to query monitor resolution; using default 1600x900");
			createWidth = createWidth > 0 ? createWidth : 1600;
			createHeight = createHeight > 0 ? createHeight : 900;
			this->m_width = createWidth;
			this->m_height = createHeight;
		}
	}

	// If explicit fullscreen requested, make sure monitor is set and override size to monitor's video mode
	if (isFullscreen) {
		if (!monitor)
			monitor = glfwGetPrimaryMonitor();

		const GLFWvidmode* vm = monitor ? glfwGetVideoMode(monitor) : nullptr;
		
		if (vm) {
			createWidth = vm->width;
			createHeight = vm->height;
			this->m_width = createWidth;
			this->m_height = createHeight;
			
			LOG_WARNING("Fullscreen requested: forcing creation size to monitor resolution: " << createWidth << "x" << createHeight);
		}

		// Mark mode as fullscreen for creation-time behavior
		mode |= WindowMode::Fullscreen;
	}

	// Ensure primary monitor is used by default for fullscreen if none provided.
	if (HasFlag(mode, WindowMode::Fullscreen) && !monitor)
		monitor = glfwGetPrimaryMonitor();

	GLFWmonitor* createMonitor = HasFlag(mode, WindowMode::Fullscreen) ? monitor : nullptr;
	m_windowHandle = glfwCreateWindow(createWidth, createHeight, title.c_str(), createMonitor, parent);
	m_title = title;

	if (!m_windowHandle) {
		// Log: "Failed to create GLFW window";
		return false;
	}
	glfwMakeContextCurrent(m_windowHandle);

	// Lock the aspect ratio (16:9)
	glfwSetWindowAspectRatio(m_windowHandle, 16, 9);

	if (!gladLoadGL()) {
		// Log: "Failed to initialize GLAD";
		glfwDestroyWindow(m_windowHandle);
		return false;
	}

	// === ENABLE OR DISABLE VSYNC HERE ===
	SetVSync(vsync);

	// Initialize input system with the window
	Input::Initialize(m_windowHandle);

	// Register all GLFW input and framebuffer callbacks
	Input::SetupEventCallbacks();

	glViewport(0, 0, this->m_width, this->m_height);

	// Store the pointer to this instance for use in callbacks
	// because GLFW does not know context
	glfwSetWindowUserPointer(m_windowHandle, this);

	// One unified framebuffer-size callback
	glfwSetFramebufferSizeCallback(m_windowHandle, [](GLFWwindow* w, int fbw, int fbh) {
		glViewport(0, 0, fbw, fbh);
		if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
			self->m_width = fbw;
			self->m_height = fbh;
		}
		Messaging::MessageSystem::Broadcast(Messaging::WindowResized{ fbw, fbh });
		});

	// Window focus callback - broadcast focus changes for audio muting, etc.
	glfwSetWindowFocusCallback(m_windowHandle, [](GLFWwindow* w, int focused) {
		(void)w;
		Messaging::MessageSystem::Broadcast(Messaging::WindowFocusChanged{ focused != 0 });
		});

	SetMode(mode, monitor);

	return true;
}

void Window::Destroy() {
	if (m_windowHandle) {
		glfwDestroyWindow(m_windowHandle);
		m_windowHandle = nullptr;
	}
}

void Window::SetMode(const WindowMode::Flags mode, GLFWmonitor* monitor) {
	// log current mode and new mode
	// Cast to int because Flags uses uint8_t (which streams as a char otherwise)
	std::cout << "Current mode: " << static_cast<int>(m_mode) << ", New mode: " << static_cast<int>(mode) << "\n";
	if (mode == m_mode) return;
	std::cout << "Switching window mode...\n";

	// TODO: When hardware manager is done, get:
	// All monitor pos and sizes, window pos (and size?)
	// Calculate them to get the monitor which the window is on
	if (!monitor) 
		monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* modeInfo = glfwGetVideoMode(monitor);
	if (!modeInfo) {
		std::cout << "Failed to query video mode for monitor\n";
		return;
	}

	std::cout << "Mode info - Width: " << modeInfo->width << ", Height: " << modeInfo->height 
			  << ", RefreshRate: " << modeInfo->refreshRate << "\n";

	if (HasFlag(mode, WindowMode::Borderless)) {
		// Store the current windowed position/size for restore
		glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
		glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
		std::cout << "Stored windowed position: (" << m_windowedX << ", " << m_windowedY << "), size: (" << m_windowedWidth << "x" << m_windowedHeight << ")\n";

		// Remove decorations
		glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_FALSE);

		// If Borderless is requested but Fullscreen is NOT requested, make the window
		// cover the monitor (borderless windowed fullscreen) without attaching to the monitor.
		if (!HasFlag(mode, WindowMode::Fullscreen)) {
			int monX = 0, monY = 0;
			glfwGetMonitorPos(monitor, &monX, &monY);
			glfwSetWindowMonitor(m_windowHandle, nullptr, monX, monY, modeInfo->width, modeInfo->height, 0);
			std::cout << "Switched to borderless windowed (covering monitor) mode\n";
		}
	}
	if (HasFlag(mode, WindowMode::Windowed)) {
		glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(m_windowHandle, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
		std::cout << "Restored windowed position: (" << m_windowedX << ", " << m_windowedY << "), size: (" << m_windowedWidth << "x" << m_windowedHeight << ")\n";
	}
	if (HasFlag(mode, WindowMode::Fullscreen)) {
		glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
		glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
		glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0, modeInfo->width, modeInfo->height, modeInfo->refreshRate);
		std::cout << "Switched to fullscreen mode on monitor\n";
	}
	
	m_mode = mode;
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
