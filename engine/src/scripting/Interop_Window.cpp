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
#include "platform/glfw/GLFWWindow.h"
#include <algorithm>

// Helper to get main window via platform context
static Platform::IWindow* GetMainWindow() {
    if (!Engine::CORE) return nullptr;
    auto* platform = Engine::CORE->GetPlatformContext();
    return platform ? platform->GetMainWindow() : nullptr;
}

// ============================================================================
// Window API - Window Queries
// ============================================================================

/**
 * @brief Get the width of the main window
 *
 * @return int Width in pixels, or 0 on error
 */
INTEROP_API int EngineInterop_Window_GetWidth() {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetWidth();
}

/**
 * @brief Get the height of the main window
 *
 * @return int Height in pixels, or 0 on error
 */
INTEROP_API int EngineInterop_Window_GetHeight() {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return 0;
    }
    return window->GetHeight();
}

/**
 * @brief Check if the window should close
 *
 * @return true if window should close or main window is unavailable
 */
INTEROP_API bool EngineInterop_Window_ShouldClose() {
    auto* window = GetMainWindow();
    if (!window) {
        return true;
    }
    return window->ShouldClose();
}

/**
 * @brief Request the window to close
 */
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

/**
 * @brief Check if the window is focused
 *
 * @return true if focused, false otherwise
 */
INTEROP_API bool EngineInterop_Window_IsFocused() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsFocused();
}

/**
 * @brief Check if the window is minimized
 *
 * @return true if minimized, false otherwise
 */
INTEROP_API bool EngineInterop_Window_IsMinimized() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMinimized();
}

/**
 * @brief Set the window minimized state
 *
 * @param minimized True to minimize, false to restore
 */
INTEROP_API void EngineInterop_Window_SetMinimized(bool minimized) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMinimized(minimized);
}

/**
 * @brief Check if the window is maximized
 *
 * @return true if maximized, false otherwise
 */
INTEROP_API bool EngineInterop_Window_IsMaximized() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsMaximized();
}

/**
 * @brief Set the window maximized state
 *
 * @param maximized True to maximize, false to restore
 */
INTEROP_API void EngineInterop_Window_SetMaximized(bool maximized) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMaximized(maximized);
}

/**
 * @brief Check if the window is visible
 *
 * @return true if visible, false otherwise
 */
INTEROP_API bool EngineInterop_Window_IsVisible() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsVisible();
}

/**
 * @brief Set the window visibility
 *
 * @param visible True to show, false to hide
 */
INTEROP_API void EngineInterop_Window_SetVisible(bool visible) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetVisible(visible);
}

/**
 * @brief Check if the window is resizable
 *
 * @return true if resizable, false otherwise
 */
INTEROP_API bool EngineInterop_Window_IsResizable() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsResizable();
}

/**
 * @brief Set the window resizable state
 *
 * @param resizable True to allow resizing, false to disable
 */
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

/**
 * @brief Set the window mode (Windowed = 1, Fullscreen = 2, Borderless = 4)
 *
 * @param mode Mode flags as integer matching Platform::WindowMode
 */
INTEROP_API void EngineInterop_Window_SetMode(int mode) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetMode(static_cast<Platform::WindowMode>(mode));
}

/**
 * @brief Check if the window has a specific mode flag
 *
 * @param mode Mode flag to check
 * @return true if the window has the mode, false otherwise
 */
INTEROP_API bool EngineInterop_Window_HasMode(int mode) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->HasMode(static_cast<Platform::WindowMode>(mode));
}

/**
 * @brief Resize the window to the specified dimensions
 *
 * @param width New width in pixels
 * @param height New height in pixels
 */
INTEROP_API void EngineInterop_Window_Resize(int width, int height) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    LOG_INFO("[ScriptAPI] Resize requested from script: " << width << "x" << height);
    window->Resize(width, height);
}

// ============================================================================
// Window API - Title and VSync
// ============================================================================

/**
 * @brief Get the window title as a UTF-8 string pointer
 *
 * @return const char* Pointer valid until the next call on this thread
 */
INTEROP_API const char* EngineInterop_Window_GetTitle() {
    thread_local std::string cachedTitle;

    auto* window = GetMainWindow();
    if (!window) {
        cachedTitle.clear();
        return cachedTitle.c_str();
    }

    cachedTitle = window->GetTitle();
    return cachedTitle.c_str();
}

/**
 * @brief Set the window title
 *
 * @param title New title string
 */
INTEROP_API void EngineInterop_Window_SetTitle(const char* title) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetTitle(title ? title : "");
}

/**
 * @brief Check if VSync is enabled
 */
INTEROP_API bool EngineInterop_Window_IsVSync() {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->IsVSync();
}

/**
 * @brief Enable or disable VSync
 */
INTEROP_API void EngineInterop_Window_SetVSync(bool enabled) {
    auto* window = GetMainWindow();
    if (!window) {
        LOG_ERROR("[ScriptAPI] Main window not available");
        return;
    }
    window->SetVSync(enabled);
}

// ============================================================================
// Window API - Fullscreen and Display Modes
// ============================================================================

/**
 * @brief Toggle fullscreen on the current monitor
 */
INTEROP_API bool EngineInterop_Window_SetFullscreen(bool fullscreen) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }
    return window->SetFullscreen(fullscreen);
}

/**
 * @brief Toggle fullscreen on a specific monitor index
 */
INTEROP_API bool EngineInterop_Window_SetFullscreenOnMonitor(int monitorIndex) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }

    if (auto* glfwWindow = dynamic_cast<Platform::GLFWWindow*>(window)) {
        return glfwWindow->SetFullscreenOnMonitor(monitorIndex);
    }

    if (monitorIndex >= 0) {
        return window->SetFullscreen(true);
    }

    return window->SetFullscreen(false);
}

/**
 * @brief Get the number of supported display modes for the current monitor
 */
INTEROP_API int EngineInterop_Window_GetSupportedDisplayModeCount() {
    auto* window = GetMainWindow();
    if (!window) {
        return 0;
    }
    return static_cast<int>(window->GetSupportedDisplayModes().size());
}

/**
 * @brief Get a supported display mode by index
 *
 * @return true if index was valid and outputs were set
 */
INTEROP_API bool EngineInterop_Window_GetSupportedDisplayMode(int index, int* outWidth, int* outHeight,
                                                              int* outRefreshRate, int* outBitsPerPixel) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }

    const auto modes = window->GetSupportedDisplayModes();
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        return false;
    }

    const auto& mode = modes[static_cast<size_t>(index)];
    if (outWidth) *outWidth = mode.Width;
    if (outHeight) *outHeight = mode.Height;
    if (outRefreshRate) *outRefreshRate = mode.RefreshRate;
    if (outBitsPerPixel) *outBitsPerPixel = mode.BitsPerPixel;
    return true;
}

/**
 * @brief Apply a display mode to the current window
 */
INTEROP_API bool EngineInterop_Window_SetDisplayMode(int width, int height, int refreshRate, int bitsPerPixel) {
    auto* window = GetMainWindow();
    if (!window) {
        return false;
    }

    Engine::DisplayMode mode{};
    mode.Width = width;
    mode.Height = height;
    mode.RefreshRate = refreshRate;
    mode.BitsPerPixel = static_cast<uint8_t>(std::max(0, bitsPerPixel));
    return window->SetDisplayMode(mode);
}
