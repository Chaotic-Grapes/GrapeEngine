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
#include <optional>

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

	// isFullscreen: if true, create an exclusive fullscreen window on the specified
	// monitor (or primary monitor if monitor==nullptr). This overrides the 'mode'
	// parameter for creation time only.
	bool Create(const std::string& title, int width, int height, bool vsync = true, bool isFullscreen = false, WindowMode::Flags mode = WindowMode::Windowed, GLFWmonitor* monitor = nullptr, GLFWwindow* parent = nullptr);
	void Destroy();

	void PollEvents() const;
	void SwapBuffers() const;
	bool ShouldClose() const;
	void Close() const;

	int GetWidth() const;
	int GetHeight() const;

	bool IsFocused() const;
	bool IsMinimized() const;
	void SetMinimized(bool flag) const;
	bool IsMaximized() const;
	void SetMaximized(bool flag) const;
	bool IsVisible() const;
	void SetVisible(bool flag) const;
	bool IsResizable() const;
	void SetResizable(bool flag) const;
	void SetVSync(bool enabled);
	bool IsVSync() const;
	
	void SetMode(WindowMode::Flags mode, GLFWmonitor* monitor = nullptr);
	bool HasMode(WindowMode::Flags value) const;
	void Resize(std::optional<int> width, std::optional<int> height);

	void SetTitle(const std::string& title);
	std::string GetTitle() const;

	GLFWwindow* Handle() const;

private:
	GLFWwindow* m_windowHandle = nullptr;
	std::string m_title;

	int m_width = 0, m_height = 0;

	// Remember windowed mode size and position for restoring
	int m_windowedX = 100, m_windowedY = 100;
	int m_windowedWidth = 1600, m_windowedHeight = 900; // TODO: Scaling?

	bool m_vsync = true;

	WindowMode::Flags m_mode = static_cast<WindowMode::Flags>(0);
};

#endif // WINDOW_H
