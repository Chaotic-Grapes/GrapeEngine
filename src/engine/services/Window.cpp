#include "services/Window.h"
#include "services/Input.h"
#include <iostream>
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

namespace {
	bool HasFlag(const WindowMode::Flags a, const WindowMode::Flags b) {
		return (a & b) != static_cast<WindowMode::Flags>(0);
	}
}

static void FramebufferSizeCallback(GLFWwindow* window, const int width, const int height) {
	(void)window;
	glViewport(0, 0, width, height);

	// Broadcast resize message
	Messaging::MessageSystem::Broadcast(Messaging::WindowResized{ width, height });
}

Window::~Window() { Destroy(); }

bool Window::Create(const std::string& title, const int width, const int height, GLFWmonitor* monitor, GLFWwindow* parent) {
	this->m_width = width;
	this->m_height = height;
	this->m_title = title;

	if (!glfwInit()) {
		// Log: "Failed to initialize GLFW";
		return false;
	}

	// In case a GLFW function fails, an error is reported to callback function
	glfwSetErrorCallback(Input::ErrorCallback);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	m_windowHandle = glfwCreateWindow(width, height, title.c_str(), monitor, parent);

	if (!m_windowHandle) {
		// Log: "Failed to create GLFW window";
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(m_windowHandle);

	// === ENABLE OR DISABLE VSYNC HERE ===
	glfwSwapInterval(1);

	if (!gladLoadGL()) {
		// Log: "Failed to initialize GLAD";
		glfwDestroyWindow(m_windowHandle);
		glfwTerminate();
		return false;
	}

	// Initialize input system with the window
	Input::Initialize(m_windowHandle);

	// Register all GLFW input and framebuffer callbacks
	Input::SetupEventCallbacks();

	glViewport(0, 0, width, height);

	// This callback function is called when the window is resized
	// to update the viewport and store the new width and height
	glfwSetFramebufferSizeCallback(m_windowHandle, [](GLFWwindow* window, const int w, const int h) {
		glViewport(0, 0, w, h);
		if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
			self->m_width = w;
			self->m_height = h;
		}
	});

	// Store the pointer to this instance for use in callbacks
	// because GLFW does not know context
	glfwSetWindowUserPointer(m_windowHandle, this);

	// Register callback for resize
	glfwSetFramebufferSizeCallback(m_windowHandle, FramebufferSizeCallback);
	return true;
}

void Window::Destroy() {
	if (m_windowHandle) {
		glfwDestroyWindow(m_windowHandle);
		m_windowHandle = nullptr;
		glfwTerminate();
	}
}

void Window::Mode(const WindowMode::Flags mode, GLFWmonitor* monitor) {
	if (mode == m_mode) return;

	// TODO: When hardware manager is done, get:
	// All monitor pos and sizes, window pos (and size?)
	// Calculate them to get the monitor which the window is on
	if (!monitor) 
		monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* modeInfo = glfwGetVideoMode(monitor);

	if (HasFlag(mode, WindowMode::Borderless)) {
		glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_FALSE);
		glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
		glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
	}
	if (HasFlag(mode, WindowMode::Windowed)) {
		glfwSetWindowAttrib(m_windowHandle, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(m_windowHandle, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
	}
	if (HasFlag(mode, WindowMode::Fullscreen)) {
		glfwGetWindowPos(m_windowHandle, &m_windowedX, &m_windowedY);
		glfwGetWindowSize(m_windowHandle, &m_windowedWidth, &m_windowedHeight);
		glfwSetWindowMonitor(m_windowHandle, monitor, 0, 0, modeInfo->width, modeInfo->height, modeInfo->refreshRate);
	}
	
	m_mode = mode;
}

void Window::Resize(const int width, const int height) {
	glfwSetWindowSize(m_windowHandle, width, height);
	m_width = width;
	m_height = height;
}

GLFWwindow* Window::Handle()    const { return m_windowHandle; }
int Window::Width()				const { return m_width; }
int Window::Height()			const { return m_height; }
void Window::PollEvents()		const { glfwPollEvents(); }
void Window::SwapBuffers()		const { glfwSwapBuffers(m_windowHandle); }
bool Window::ShouldClose()		const { return m_windowHandle && glfwWindowShouldClose(m_windowHandle); }
void Window::Close()			const { glfwSetWindowShouldClose(m_windowHandle, true); }

bool Window::IsFocused()				  const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_FOCUSED); }
bool Window::IsMinimized()				  const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_ICONIFIED); }
void Window::IsMinimized(const bool flag) const { if (m_windowHandle) glfwSetWindowAttrib(m_windowHandle, GLFW_ICONIFIED, flag); }
bool Window::IsMaximized()				  const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_MAXIMIZED); }
void Window::IsMaximized(const bool flag) const { if (m_windowHandle) glfwSetWindowAttrib(m_windowHandle, GLFW_MAXIMIZED, flag); }
bool Window::IsVisible()				  const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_VISIBLE); }
void Window::IsVisible(const bool flag)	  const { if (m_windowHandle) glfwSetWindowAttrib(m_windowHandle, GLFW_VISIBLE, flag); }
bool Window::IsResizable()				  const { return m_windowHandle && glfwGetWindowAttrib(m_windowHandle, GLFW_RESIZABLE); }
void Window::IsResizable(const bool flag) const { if (m_windowHandle) glfwSetWindowAttrib(m_windowHandle, GLFW_RESIZABLE, flag); }

bool Window::HasMode(const WindowMode::Flags value) const { return HasFlag(value, m_mode); }
