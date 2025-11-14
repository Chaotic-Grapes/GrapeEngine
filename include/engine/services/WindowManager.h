/* Start Header *****************************************************************/
/*!
\file   WindowManager.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th October 2025
\brief
Window management utilities for the engine. Provides functions to create,
destroy, and manage windows.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#define CREATE_WINDOW(title, width, height) WindowManager::CreateWindow(title, width, height)
#define CREATE_CHILD_WINDOW(title, width, height, parent) WindowManager::CreateWindow(title, width, height, parent)
#define DESTROY_WINDOW(window) WindowManager::DestroyWindow(window)
#define DESTROY_ALL_WINDOWS() WindowManager::DestroyAll();

#include <vector>
#include "Window.h"

#ifdef CreateWindow
#undef CreateWindow
#endif

class WindowManager final {
public:
    ~WindowManager();

    // Create a new window, or a child window
    static Window* CreateWindow(const std::string& title, int width, int height, const Window* parent = nullptr);

    // Destroy a specific window
    static void DestroyWindow(const Window* window);

    // Destroy all windows
    static void DestroyAll();

    static const std::vector<Window*>& GetWindows();
    static Window* GetMainWindow();

private:
    static std::vector<Window*> m_windows;
};

#endif