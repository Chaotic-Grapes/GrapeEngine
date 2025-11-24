/* Start Header *****************************************************************/
/*!
\file   WindowManager.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Implements the WindowManager service which manages multiple application windows.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/WindowManager.h"
#include "services/Input.h"
#include <memory>
#include <algorithm>
#include <iostream>

namespace {
    // Tracks whether glfw has been initialized by the WindowManager
    static bool s_glfwInitialized = false;
}

std::vector<std::unique_ptr<Window>> WindowManager::m_windows;

WindowManager::~WindowManager() {
    DestroyAll();
}

Window* WindowManager::CreateWindow(const std::string& title, const int width, const int height, bool vsync, WindowMode::Flags mode, const Window* parent) {
    // Ensure GLFW is initialized once before attempting to create any window
    if (!s_glfwInitialized) {
        if (!glfwInit()) {
            // Initialization failed
            return nullptr;
        }
        // Set global error callback once
        glfwSetErrorCallback(Input::ErrorCallback);
        s_glfwInitialized = true;
    }

    // Create a window owned by unique_ptr so WindowManager keeps ownership
    auto window = std::make_unique<Window>();
    // TODO: Hardware manager before supporting glfwGetPrimaryMonitor()
    if (window->Create(title, width, height, vsync, mode, /*glfwGetPrimaryMonitor()*/ nullptr, parent ? parent->Handle() : nullptr)) {
        Window* raw = window.get();
        m_windows.push_back(std::move(window));
        return raw;
    }
    // unique_ptr will free on scope exit
    return nullptr;
}

void WindowManager::DestroyWindow(const Window* window) {
    if (!window) return;

    // TODO: Destroy all child windows first if parent window is being destroyed
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [window](const std::unique_ptr<Window>& uptr) {
        return uptr.get() == window;
    });
    if (it != m_windows.end()) {
        (*it)->Destroy();
        m_windows.erase(it);
    }
}

void WindowManager::DestroyAll() {
    for (auto& window : m_windows) {
        if (window) window->Destroy();
    }
    m_windows.clear();

    // Terminate GLFW when all windows are destroyed
    if (s_glfwInitialized) {
        glfwTerminate();
        s_glfwInitialized = false;
    }
}

std::vector<Window*> WindowManager::GetWindows() {
    std::vector<Window*> out;
    out.reserve(m_windows.size());
    for (const auto& up : m_windows) out.push_back(up.get());
    return out;
}

Window* WindowManager::GetMainWindow() { return m_windows.empty() ? nullptr : m_windows.front().get(); }

