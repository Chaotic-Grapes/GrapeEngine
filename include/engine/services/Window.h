/* Start Header *****************************************************************/
/*!
\file   Window.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Window management utilities for the engine. Provides functions to create,
destroy, and manage windows.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef WINDOW_H
#define WINDOW_H

#include <string>
#include <glad/glad.h> // DO NOT REMOVE THIS LINE OR IT WILL NOT COMPILE
#include <GLFW/glfw3.h>

#pragma region WindowMode enum
struct WindowMode {
	enum Flags : uint8_t {
		Windowed	= 1 << 0,
		Fullscreen	= 1 << 1,
		Borderless	= 1 << 2
	};

	using Type = std::underlying_type_t<Flags>;
};

// ******************************** Bitwise operators for WindowMode::Flags ******************************** //
inline WindowMode::Flags operator|(const WindowMode::Flags a, const WindowMode::Flags b) {
	return static_cast<WindowMode::Flags>(static_cast<WindowMode::Type>(a) | static_cast<WindowMode::Type>(b));
}

inline WindowMode::Flags operator&(const WindowMode::Flags a, const WindowMode::Flags b) {
	return static_cast<WindowMode::Flags>(static_cast<WindowMode::Type>(a) & static_cast<WindowMode::Type>(b));
}

inline WindowMode::Flags& operator|=(WindowMode::Flags& a, const WindowMode::Flags b) {
	a = a | b;
	return a;
}
#pragma endregion

// Todo: Borderless, Fullscreen etc
// Borderless: glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
class Window {
public:	
	Window() = default;
	~Window();

	bool Create(const std::string& title, int width, int height, GLFWmonitor* monitor = nullptr, GLFWwindow* parent = nullptr);
	void Destroy();

	void PollEvents() const;
	void SwapBuffers() const;
	bool ShouldClose() const;
	void Close() const;

	int Width() const;
	int Height() const;

	bool IsFocused() const;
	bool IsMinimized() const;
	void IsMinimized(bool flag) const;
	bool IsMaximized() const;
	void IsMaximized(bool flag) const;
	bool IsVisible() const;
	void IsVisible(bool flag) const;
	bool IsResizable() const;
	void IsResizable(bool flag) const;
	
	void Mode(WindowMode::Flags mode, GLFWmonitor* monitor = nullptr);
	bool HasMode(WindowMode::Flags value) const;
	void Resize(int width, int height);

	GLFWwindow* Handle() const;

private:
	GLFWwindow* m_windowHandle = nullptr;
	std::string m_title;

	int m_width = 0, m_height = 0;

	// Remember windowed mode size and position for restoring
	int m_windowedX = 100, m_windowedY = 100;
	int m_windowedWidth = 1280, m_windowedHeight = 720; // TODO: Scaling?

	WindowMode::Flags m_mode = WindowMode::Windowed;
};

#endif // WINDOW_H
