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

const std::vector<Window*>& WindowManager::GetWindows()       { return m_windows; }
Window* WindowManager::GetMainWindow()                        { return m_windows.empty() ? nullptr : m_windows.front(); }

