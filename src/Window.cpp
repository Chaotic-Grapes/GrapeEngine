#include "Window.h"
#include "Input.h"
#include <iostream>

Window::~Window() { Destroy(); }

bool Window::Create(const std::string& title, const int width, const int height, GLFWmonitor* monitor) {
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

	m_windowHandle = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);

	if (!m_windowHandle) {
		// Log: "Failed to create GLFW window";
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(m_windowHandle);
	if (!gladLoadGL()) {
		// Log: "Failed to initialize GLAD";
		glfwDestroyWindow(m_windowHandle);
		glfwTerminate();
		return false;
	}

	// Initialize input system with the window
	Input::Init(m_windowHandle);

	// Register all GLFW input and framebuffer callbacks
	Input::SetupEventCallbacks();

	glViewport(0, 0, width, height);
	return true;
}

void Window::Destroy() {
	if (m_windowHandle) {
		glfwDestroyWindow(m_windowHandle);
		m_windowHandle = nullptr;
		glfwTerminate();
	}
}

GLFWwindow* Window::GetWindow() const { return m_windowHandle; }
int Window::Width()				const { return m_width; }
int Window::Height()			const { return m_height; }
void Window::PollEvents()		const { glfwPollEvents(); }
void Window::SwapBuffers()		const { glfwSwapBuffers(m_windowHandle); }
bool Window::ShouldClose()		const { return m_windowHandle && glfwWindowShouldClose(m_windowHandle); }
void Window::Close()			const { glfwSetWindowShouldClose(m_windowHandle, true); }
