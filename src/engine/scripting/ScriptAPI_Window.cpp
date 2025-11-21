/* Start Header *****************************************************************/
/*!
\file    ScriptAPI_Window.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    21st November 2025
\brief
C# scripting API exports for window management and queries.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/WindowManager.h"
#include "services/Window.h"
#include "core/Logger.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// ============================================================================
// Window API - Window Queries
// ============================================================================

/// <summary>
/// Get the width of the main window
/// </summary>
SCRIPT_API int ScriptAPI_Window_GetWidth() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->Width();
}

/// <summary>
/// Get the height of the main window
/// </summary>
SCRIPT_API int ScriptAPI_Window_GetHeight() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->Height();
}

/// <summary>
/// Check if the window should close
/// </summary>
SCRIPT_API bool ScriptAPI_Window_ShouldClose() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return true;
    }
    return window->ShouldClose();
}

/// <summary>
/// Request the window to close
/// </summary>
SCRIPT_API void ScriptAPI_Window_Close() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->Close();
}

// ============================================================================
// Window API - Window State
// ============================================================================

/// <summary>
/// Check if the window is focused
/// </summary>
SCRIPT_API bool ScriptAPI_Window_IsFocused() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsFocused();
}

/// <summary>
/// Check if the window is minimized
/// </summary>
SCRIPT_API bool ScriptAPI_Window_IsMinimized() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMinimized();
}

/// <summary>
/// Set the window minimized state
/// </summary>
SCRIPT_API void ScriptAPI_Window_SetMinimized(bool minimized) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->IsMinimized(minimized);
}

/// <summary>
/// Check if the window is maximized
/// </summary>
SCRIPT_API bool ScriptAPI_Window_IsMaximized() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMaximized();
}

/// <summary>
/// Set the window maximized state
/// </summary>
SCRIPT_API void ScriptAPI_Window_SetMaximized(bool maximized) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->IsMaximized(maximized);
}

/// <summary>
/// Check if the window is visible
/// </summary>
SCRIPT_API bool ScriptAPI_Window_IsVisible() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsVisible();
}

/// <summary>
/// Set the window visibility
/// </summary>
SCRIPT_API void ScriptAPI_Window_SetVisible(bool visible) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->IsVisible(visible);
}

/// <summary>
/// Check if the window is resizable
/// </summary>
SCRIPT_API bool ScriptAPI_Window_IsResizable() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsResizable();
}

/// <summary>
/// Set the window resizable state
/// </summary>
SCRIPT_API void ScriptAPI_Window_SetResizable(bool resizable) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->IsResizable(resizable);
}

// ============================================================================
// Window API - Window Mode and Size
// ============================================================================

/// <summary>
/// Set the window mode (Windowed = 1, Fullscreen = 2, Borderless = 4)
/// </summary>
SCRIPT_API void ScriptAPI_Window_SetMode(int mode) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->Mode(static_cast<WindowMode::Flags>(mode), nullptr);
}

/// <summary>
/// Check if the window has a specific mode flag
/// </summary>
SCRIPT_API bool ScriptAPI_Window_HasMode(int mode) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->HasMode(static_cast<WindowMode::Flags>(mode));
}

/// <summary>
/// Resize the window to the specified dimensions
/// </summary>
SCRIPT_API void ScriptAPI_Window_Resize(int width, int height) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->Resize(width, height);
}
