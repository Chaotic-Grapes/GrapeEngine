/* Start Header *****************************************************************/
/*!
\file    Interop_Window.cpp
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

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "core/Application.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "core/Logger.h"

// Helper to get main window via platform context
static Platform::IWindow* GetMainWindow() {
    if (!Engine::CORE) return nullptr;
    auto* platform = Engine::CORE->GetPlatformContext();
    return platform ? platform->GetMainWindow() : nullptr;
}

// ============================================================================
// Window API - Window Queries
// ============================================================================

/// <summary>
/// Get the width of the main window
/// </summary>
INTEROP_API int EngineInterop_Window_GetWidth() {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetWidth();
}

/// <summary>
/// Get the height of the main window
/// </summary>
INTEROP_API int EngineInterop_Window_GetHeight() {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetHeight();
}

/// <summary>
/// Check if the window should close
/// </summary>
INTEROP_API bool EngineInterop_Window_ShouldClose() {
    auto* window = GetMainWindow();
    if (!window) {
        return true;
    }
    return window->ShouldClose();
}

/// <summary>
/// Request the window to close
/// </summary>
INTEROP_API void EngineInterop_Window_Close() {
    auto* window = GetMainWindow();
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
INTEROP_API bool EngineInterop_Window_IsFocused() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsFocused();
}

/// <summary>
/// Check if the window is minimized
/// </summary>
INTEROP_API bool EngineInterop_Window_IsMinimized() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMinimized();
}

/// <summary>
/// Set the window minimized state
/// </summary>
INTEROP_API void EngineInterop_Window_SetMinimized(bool minimized) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMinimized(minimized);
}

/// <summary>
/// Check if the window is maximized
/// </summary>
INTEROP_API bool EngineInterop_Window_IsMaximized() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMaximized();
}

/// <summary>
/// Set the window maximized state
/// </summary>
INTEROP_API void EngineInterop_Window_SetMaximized(bool maximized) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMaximized(maximized);
}

/// <summary>
/// Check if the window is visible
/// </summary>
INTEROP_API bool EngineInterop_Window_IsVisible() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsVisible();
}

/// <summary>
/// Set the window visibility
/// </summary>
INTEROP_API void EngineInterop_Window_SetVisible(bool visible) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetVisible(visible);
}

/// <summary>
/// Check if the window is resizable
/// </summary>
INTEROP_API bool EngineInterop_Window_IsResizable() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsResizable();
}

/// <summary>
/// Set the window resizable state
/// </summary>
INTEROP_API void EngineInterop_Window_SetResizable(bool resizable) {
    auto* window = GetMainWindow();
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
INTEROP_API void EngineInterop_Window_SetMode(int mode) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMode(static_cast<Platform::WindowMode>(mode));
}

/// <summary>
/// Check if the window has a specific mode flag
/// </summary>
INTEROP_API bool EngineInterop_Window_HasMode(int mode) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->HasMode(static_cast<Platform::WindowMode>(mode));
}

/// <summary>
/// Resize the window to the specified dimensions
/// </summary>
INTEROP_API void EngineInterop_Window_Resize(int width, int height) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    LOG_INFO("[ScriptAPI] Resize requested from script: " << width << "x" << height);
    window->Resize(width, height);
}
