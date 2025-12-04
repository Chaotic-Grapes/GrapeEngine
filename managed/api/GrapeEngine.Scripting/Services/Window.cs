/* Start Header *****************************************************************/
/*!
\file   Window.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides access to window management and queries.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


namespace GrapeEngine;

/// <summary>
/// Window mode flags.
/// </summary>
[Flags]
public enum WindowMode
{
    Windowed = 1,
    Fullscreen = 2,
    Borderless = 4
}

/// <summary>
/// Provides access to window management and queries.
/// </summary>
public static class Window
{
    // ============================================================================
    // Window Dimensions
    // ============================================================================

    /// <summary>
    /// Get the current window width in pixels.
    /// </summary>
    public static int Width => WindowAPI.GetWidth();

    /// <summary>
    /// Get the current window height in pixels.
    /// </summary>
    public static int Height => WindowAPI.GetHeight();

    /// <summary>
    /// Resize the window to the specified dimensions.
    /// </summary>
    public static void Resize(int width, int height) => WindowAPI.Resize(width, height);

    // ============================================================================
    // Window State
    // ============================================================================

    /// <summary>
    /// Check if the window should close.
    /// </summary>
    public static bool ShouldClose => WindowAPI.ShouldClose();

    /// <summary>
    /// Request the window to close.
    /// </summary>
    public static void Close() => WindowAPI.Close();

    /// <summary>
    /// Check if the window is currently focused.
    /// </summary>
    public static bool IsFocused => WindowAPI.IsFocused();

    /// <summary>
    /// Get or set whether the window is minimized.
    /// </summary>
    public static bool IsMinimized
    {
        get => WindowAPI.IsMinimized();
        set => WindowAPI.SetMinimized(value);
    }

    /// <summary>
    /// Get or set whether the window is maximized.
    /// </summary>
    public static bool IsMaximized
    {
        get => WindowAPI.IsMaximized();
        set => WindowAPI.SetMaximized(value);
    }

    /// <summary>
    /// Get or set whether the window is visible.
    /// </summary>
    public static bool IsVisible
    {
        get => WindowAPI.IsVisible();
        set => WindowAPI.SetVisible(value);
    }

    /// <summary>
    /// Get or set whether the window is resizable.
    /// </summary>
    public static bool IsResizable
    {
        get => WindowAPI.IsResizable();
        set => WindowAPI.SetResizable(value);
    }

    // ============================================================================
    // Window Mode
    // ============================================================================

    /// <summary>
    /// Set the window mode.
    /// </summary>
    public static void SetMode(WindowMode mode) => WindowAPI.SetMode((int)mode);

    /// <summary>
    /// Check if the window has a specific mode flag.
    /// </summary>
    public static bool HasMode(WindowMode mode) => WindowAPI.HasMode((int)mode);
}
