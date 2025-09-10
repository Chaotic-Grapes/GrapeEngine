#include "Window.h"
#include <iostream>

Window::~Window() { Destroy(); }

bool Window::Create(const std::string& title, const int width, const int height, GLFWmonitor* monitor) {
	this->WindowWidth = width;
	this->WindowHeight = height;
	this->WindowTitle = title;

	if (!glfwInit()) {
		// Log: "Failed to initialize GLFW";
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	WindowHandle = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);

	if (!WindowHandle) {
		// Log: "Failed to create GLFW window";
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(WindowHandle);
	if (!gladLoadGL()) {
		// Log: "Failed to initialize GLAD";
		glfwDestroyWindow(WindowHandle);
		glfwTerminate();
		return false;
	}
	glViewport(0, 0, width, height);
	return true;
}

void Window::Destroy() {
	if (WindowHandle) {
		glfwDestroyWindow(WindowHandle);
		WindowHandle = nullptr;
		glfwTerminate();
	}
}

GLFWwindow* Window::GetWindow() const { return WindowHandle; }
int Window::Width()				const { return WindowWidth; }
int Window::Height()			const { return WindowHeight; }
void Window::PollEvents()		const { glfwPollEvents(); }
void Window::SwapBuffers()		const { glfwSwapBuffers(WindowHandle); }
bool Window::ShouldClose()		const { return WindowHandle && glfwWindowShouldClose(WindowHandle); }
void Window::Close()			const { glfwSetWindowShouldClose(WindowHandle, true); }
