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
using GrapeEngine.Scripting.Internal.Profiling;

namespace GrapeEngine.Scripting.Services;

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
    public static int Width
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.GetWidth"))
            {
                return WindowAPI.GetWidth();
            }
        }
    }

    /// <summary>
    /// Get the current window height in pixels.
    /// </summary>
    public static int Height
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.GetHeight"))
            {
                return WindowAPI.GetHeight();
            }
        }
    }

    /// <summary>
    /// Resize the window to the specified dimensions.
    /// </summary>
    public static void Resize(int width, int height)
    {
        using (PInvokeTimer.Start("WindowAPI.Resize"))
        {
            WindowAPI.Resize(width, height);
        }
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
            using (PInvokeTimer.Start("WindowAPI.ShouldClose"))
            {
                return WindowAPI.ShouldClose();
            }
        }
    }

    /// <summary>
    /// Request the window to close.
    /// </summary>
    public static void Close()
    {
        using (PInvokeTimer.Start("WindowAPI.Close"))
        {
            WindowAPI.Close();
        }
    }

    /// <summary>
    /// Check if the window is currently focused.
    /// </summary>
    public static bool IsFocused
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.IsFocused"))
            {
                return WindowAPI.IsFocused();
            }
        }
    }

    /// <summary>
    /// Get or set whether the window is minimized.
    /// </summary>
    public static bool IsMinimized
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.IsMinimized"))
            {
                return WindowAPI.IsMinimized();
            }
        }
        set
        {
            using (PInvokeTimer.Start("WindowAPI.SetMinimized"))
            {
                WindowAPI.SetMinimized(value);
            }
        }
    }

    /// <summary>
    /// Get or set whether the window is maximized.
    /// </summary>
    public static bool IsMaximized
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.IsMaximized"))
            {
                return WindowAPI.IsMaximized();
            }
        }
        set
        {
            using (PInvokeTimer.Start("WindowAPI.SetMaximized"))
            {
                WindowAPI.SetMaximized(value);
            }
        }
    }

    /// <summary>
    /// Get or set whether the window is visible.
    /// </summary>
    public static bool IsVisible
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.IsVisible"))
            {
                return WindowAPI.IsVisible();
            }
        }
        set
        {
            using (PInvokeTimer.Start("WindowAPI.SetVisible"))
            {
                WindowAPI.SetVisible(value);
            }
        }
    }

    /// <summary>
    /// Get or set whether the window is resizable.
    /// </summary>
    public static bool IsResizable
    {
        get
        {
            using (PInvokeTimer.Start("WindowAPI.IsResizable"))
            {
                return WindowAPI.IsResizable();
            }
        }
        set
        {
            using (PInvokeTimer.Start("WindowAPI.SetResizable"))
            {
                WindowAPI.SetResizable(value);
            }
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
        using (PInvokeTimer.Start("WindowAPI.SetMode"))
        {
            WindowAPI.SetMode((int)mode);
        }
    }

    /// <summary>
    /// Check if the window has a specific mode flag.
    /// </summary>
    public static bool HasMode(WindowMode mode)
    {
        using (PInvokeTimer.Start("WindowAPI.HasMode"))
        {
            return WindowAPI.HasMode((int)mode);
        }
    }
}

