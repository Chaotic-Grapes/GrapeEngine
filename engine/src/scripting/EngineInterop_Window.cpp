/* Start Header *****************************************************************/
/*!
\file    EngineInterop_Window.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    21st November 2025
\brief
C API exports for managed C# scripting systems for window management and queries.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/WindowManager.h"
#include "services/Window.h"
#include "core/Logger.h"

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_ENGINE_INTEROP
        #define ENGINE_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define ENGINE_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define ENGINE_INTEROP_API extern "C"
#endif

// ============================================================================
// Window API - Window Queries
// ============================================================================

/// <summary>
/// Get the width of the main window
/// </summary>
ENGINE_INTEROP_API int EngineInterop_Window_GetWidth() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetWidth();
}

/// <summary>
/// Get the height of the main window
/// </summary>
ENGINE_INTEROP_API int EngineInterop_Window_GetHeight() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetHeight();
}

/// <summary>
/// Check if the window should close
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_ShouldClose() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return true;
    }
    return window->ShouldClose();
}

/// <summary>
/// Request the window to close
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_Close() {
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
ENGINE_INTEROP_API bool EngineInterop_Window_IsFocused() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsFocused();
}

/// <summary>
/// Check if the window is minimized
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_IsMinimized() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMinimized();
}

/// <summary>
/// Set the window minimized state
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_SetMinimized(bool minimized) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMinimized(minimized);
}

/// <summary>
/// Check if the window is maximized
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_IsMaximized() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMaximized();
}

/// <summary>
/// Set the window maximized state
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_SetMaximized(bool maximized) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMaximized(maximized);
}

/// <summary>
/// Check if the window is visible
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_IsVisible() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsVisible();
}

/// <summary>
/// Set the window visibility
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_SetVisible(bool visible) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetVisible(visible);
}

/// <summary>
/// Check if the window is resizable
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_IsResizable() {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsResizable();
}

/// <summary>
/// Set the window resizable state
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_SetResizable(bool resizable) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetResizable(resizable);
}

// ============================================================================
// Window API - Window Mode and Size
// ============================================================================

/// <summary>
/// Set the window mode (Windowed = 1, Fullscreen = 2, Borderless = 4)
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_SetMode(int mode) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMode(static_cast<WindowMode::Flags>(mode), nullptr);
}

/// <summary>
/// Check if the window has a specific mode flag
/// </summary>
ENGINE_INTEROP_API bool EngineInterop_Window_HasMode(int mode) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        return false;
    }
    return window->HasMode(static_cast<WindowMode::Flags>(mode));
}

/// <summary>
/// Resize the window to the specified dimensions
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Window_Resize(int width, int height) {
    Window* window = WindowManager::GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    LOG_INFO("[ScriptAPI] Resize requested from script: " << width << "x" << height);
    window->Resize(width, height);
}
