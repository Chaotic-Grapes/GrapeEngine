#ifndef WINDOW_H
#define WINDOW_H

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Window
{
public:
	Window() = default;
	~Window();

	bool Create(const std::string& title, int width, int height, GLFWmonitor* monitor = nullptr);
	void Destroy();

	void PollEvents() const;
	void SwapBuffers() const;
	bool ShouldClose() const;
	void Close() const;

	int Width() const;
	int Height() const;

	GLFWwindow* GetWindow() const;

private:
	GLFWwindow* WindowHandle = nullptr;
	int WindowWidth = 0;
	int WindowHeight = 0;
	std::string WindowTitle;
};

#endif // WINDOW_H
