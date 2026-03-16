/* Start Header *****************************************************************/
/*!
\file   Input.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   28th October 2025
\brief
This file contains the implementation of the Input static class,
which provides access to keyboard and mouse input.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Provides access to keyboard and mouse input.
/// This is a static wrapper around the native Input service.
/// </summary>
public static class Input
{
    // ============================================================================
    // Keyboard Input
    // ============================================================================

    /// <summary>
    /// Check if a key is currently being pressed.
    /// </summary>
    /// <param name="key">The key code to check.</param>
    /// <returns>True if the key was just pressed this frame.</returns>
    public static bool IsKeyPressed(int key)
    {
        return InputAPI.IsKeyPressed(key);
    }

    /// <summary>
    /// Check if a key is currently held down.
    /// </summary>
    /// <param name="key">The key code to check.</param>
    /// <returns>True if the key is currently down.</returns>
    public static bool IsKeyDown(int key)
    {
        return InputAPI.IsKeyDown(key);
    }

    /// <summary>
    /// Check if a key was just released this frame.
    /// </summary>
    /// <param name="key">The key code to check.</param>
    /// <returns>True if the key was released this frame.</returns>
    public static bool IsKeyUp(int key)
    {
        return InputAPI.IsKeyUp(key);
    }

    // ============================================================================
    // Mouse Input
    // ============================================================================

    /// <summary>
    /// Check if a mouse button is currently pressed.
    /// </summary>
    /// <param name="button">The mouse button to check.</param>
    /// <returns>True if the button is currently pressed.</returns>
    public static bool IsMousePressed(int button)
    {
        return InputAPI.IsMousePressed(button);
    }

    /// <summary>
    /// Get the current mouse X coordinate in window space.
    /// </summary>
    public static double MouseX
    {
        get
        {
            return InputAPI.GetMouseX();
        }
    }

    /// <summary>
    /// Get the current mouse Y coordinate in window space.
    /// </summary>
    public static double MouseY
    {
        get
        {
            return InputAPI.GetMouseY();
        }
    }

    /// <summary>
    /// Get the current horizontal scroll offset.
    /// </summary>
    public static double ScrollX
    {
        get
        {
            return InputAPI.GetScrollX();
        }
    }

    /// <summary>
    /// Get the current vertical scroll offset.
    /// </summary>
    public static double ScrollY
    {
        get
        {
            return InputAPI.GetScrollY();
        }
    }
}

