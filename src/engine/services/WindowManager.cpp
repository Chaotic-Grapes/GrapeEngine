#include "services/WindowManager.h"
#include <iostream>

std::vector<Window*> WindowManager::m_windows;

WindowManager::~WindowManager() {
    DestroyAll();
}

Window* WindowManager::CreateWindow(const std::string& title, const int width, const int height, const Window* parent) {
    const auto window = new Window();
    // TODO: Hardware manager before supporting glfwGetPrimaryMonitor()
    if (window->Create(title, width, height, /*glfwGetPrimaryMonitor()*/ nullptr, parent ? parent->Handle() : nullptr)) { 
        m_windows.push_back(window);
        return window;
    }
    delete window;
    return nullptr;
}

void WindowManager::DestroyWindow(const Window* window) {
    if (!window) return;

	// TODO: Destroy all child windows first if parent window is being destroyed
    if (const auto it = std::find(m_windows.begin(), m_windows.end(), window); it != m_windows.end()) {
        (*it)->Destroy();
        delete *it;
        m_windows.erase(it);
    }
}

void WindowManager::DestroyAll() {
    for (auto* window : m_windows) {
        window->Destroy();
        delete window;
    }
    m_windows.clear();
}

void WindowManager::OnCreate() {
    // Create main window
    if (m_windows.empty()) {
        CreateWindow("GrapeEngine", 1920, 1080);
    }
}

void WindowManager::OnUpdate() {
    // Poll events once (GLFW applies to all windows)
    glfwPollEvents();

    for (const auto* window : m_windows) {
        if (!window->ShouldClose()) {
            window->SwapBuffers();
        }
    }
}

std::string WindowManager::Name()                       const { return "WindowManager"; }
const std::vector<Window*>& WindowManager::GetWindows()       { return m_windows; }
Window* WindowManager::GetMainWindow()                        { return m_windows.empty() ? nullptr : m_windows.front(); }

