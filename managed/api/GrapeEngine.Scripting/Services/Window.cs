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


using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Core;
using System;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Services;

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
    public static int Width
    {
        get
        {
            return WindowAPI.GetWidth();
        }
    }

    /// <summary>
    /// Get the current window height in pixels.
    /// </summary>
    public static int Height
    {
        get
        {
            return WindowAPI.GetHeight();
        }
    }

    /// <summary>
    /// Resize the window to the specified dimensions.
    /// </summary>
    public static void Resize(int width, int height)
    {
        WindowAPI.Resize(width, height);
    }

    // ============================================================================
    // Window State
    // ============================================================================

    /// <summary>
    /// Check if the window should close.
    /// </summary>
    public static bool ShouldClose
    {
        get
        {
            return WindowAPI.ShouldClose();
        }
    }

    /// <summary>
    /// Request the window to close.
    /// </summary>
    public static void Close()
    {
        WindowAPI.Close();
    }

    /// <summary>
    /// Check if the window is currently focused.
    /// </summary>
    public static bool IsFocused
    {
        get
        {
            return WindowAPI.IsFocused();
        }
    }

    /// <summary>
    /// Get or set whether the window is minimized.
    /// </summary>
    public static bool IsMinimized
    {
        get
        {
            return WindowAPI.IsMinimized();
        }
        set
        {
            WindowAPI.SetMinimized(value);
        }
    }

    /// <summary>
    /// Get or set whether the window is maximized.
    /// </summary>
    public static bool IsMaximized
    {
        get
        {
            return WindowAPI.IsMaximized();
        }
        set
        {
            WindowAPI.SetMaximized(value);
        }
    }

    /// <summary>
    /// Get or set whether the window is visible.
    /// </summary>
    public static bool IsVisible
    {
        get
        {
            return WindowAPI.IsVisible();
        }
        set
        {
            WindowAPI.SetVisible(value);
        }
    }

    /// <summary>
    /// Get or set whether the window is resizable.
    /// </summary>
    public static bool IsResizable
    {
        get
        {
            return WindowAPI.IsResizable();
        }
        set
        {
            WindowAPI.SetResizable(value);
        }
    }

    // ============================================================================
    // Window Mode
    // ============================================================================

    /// <summary>
    /// Set the window mode.
    /// </summary>
    public static void SetMode(WindowMode mode)
    {
        WindowAPI.SetMode((int)mode);
    }

    /// <summary>
    /// Check if the window has a specific mode flag.
    /// </summary>
    public static bool HasMode(WindowMode mode)
    {
        return WindowAPI.HasMode((int)mode);
    }

    // ============================================================================
    // Title and VSync
    // ============================================================================

    /// <summary>
    /// Get or set the window title.
    /// </summary>
    public static string Title
    {
        get
        {
            IntPtr titlePtr = WindowAPI.GetTitle();
            return Marshal.PtrToStringUTF8(titlePtr) ?? string.Empty;
        }
        set
        {
            WindowAPI.SetTitle(value ?? string.Empty);
        }
    }

    /// <summary>
    /// Get or set whether VSync is enabled.
    /// </summary>
    public static bool IsVSync
    {
        get
        {
            return WindowAPI.IsVSync();
        }
        set
        {
            WindowAPI.SetVSync(value);
        }
    }

    // ============================================================================
    // Fullscreen and Display Modes
    // ============================================================================

    /// <summary>
    /// Toggle fullscreen on the current monitor.
    /// </summary>
    public static bool SetFullscreen(bool fullscreen)
    {
        return WindowAPI.SetFullscreen(fullscreen);
    }

    /// <summary>
    /// Toggle fullscreen on a specific monitor index.
    /// </summary>
    public static bool SetFullscreenOnMonitor(int monitorIndex)
    {
        return WindowAPI.SetFullscreenOnMonitor(monitorIndex);
    }

    /// <summary>
    /// Get supported display modes for the current monitor.
    /// </summary>
    public static DisplayMode[] GetSupportedDisplayModes()
    {
        int count = WindowAPI.GetSupportedDisplayModeCount();
        if (count <= 0)
        {
            return Array.Empty<DisplayMode>();
        }

        var modes = new DisplayMode[count];
        int written = 0;

        for (int i = 0; i < count; i++)
        {
            if (WindowAPI.GetSupportedDisplayMode(i, out int width, out int height,
                    out int refreshRate, out int bitsPerPixel))
            {
                modes[written++] = new DisplayMode(width, height, refreshRate, bitsPerPixel);
            }
        }

        if (written == count)
        {
            return modes;
        }

        var trimmed = new DisplayMode[written];
        Array.Copy(modes, trimmed, written);
        return trimmed;
    }

    /// <summary>
    /// Apply a display mode to the current window.
    /// </summary>
    public static bool SetDisplayMode(DisplayMode mode)
    {
        return WindowAPI.SetDisplayMode(mode.Width, mode.Height, mode.RefreshRate, mode.BitsPerPixel);
    }
}

