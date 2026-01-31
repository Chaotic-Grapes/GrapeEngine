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

// ============================================================================
// Input Constants - Key Codes
// ============================================================================

/// <summary>
/// Keyboard key codes.
/// </summary>
public static class KeyCode
{
    // Letters
    public const int A = 65;
    public const int B = 66;
    public const int C = 67;
    public const int D = 68;
    public const int E = 69;
    public const int F = 70;
    public const int G = 71;
    public const int H = 72;
    public const int I = 73;
    public const int J = 74;
    public const int K = 75;
    public const int L = 76;
    public const int M = 77;
    public const int N = 78;
    public const int O = 79;
    public const int P = 80;
    public const int Q = 81;
    public const int R = 82;
    public const int S = 83;
    public const int T = 84;
    public const int U = 85;
    public const int V = 86;
    public const int W = 87;
    public const int X = 88;
    public const int Y = 89;
    public const int Z = 90;

    // Numbers
    public const int Alpha0 = 48;
    public const int Alpha1 = 49;
    public const int Alpha2 = 50;
    public const int Alpha3 = 51;
    public const int Alpha4 = 52;
    public const int Alpha5 = 53;
    public const int Alpha6 = 54;
    public const int Alpha7 = 55;
    public const int Alpha8 = 56;
    public const int Alpha9 = 57;

    // Special keys
    public const int Space = 32;
    public const int Apostrophe = 39;
    public const int Comma = 44;
    public const int Minus = 45;
    public const int Period = 46;
    public const int Slash = 47;
    public const int Semicolon = 59;
    public const int Equal = 61;

    // Function keys
    public const int Escape = 256;
    public const int Enter = 257;
    public const int Tab = 258;
    public const int Backspace = 259;
    public const int Insert = 260;
    public const int Delete = 261;

    // Arrow keys
    public const int Right = 262;
    public const int Left = 263;
    public const int Down = 264;
    public const int Up = 265;

    // Numpad
    public const int Keypad0 = 320;
    public const int Keypad1 = 321;
    public const int Keypad2 = 322;
    public const int Keypad3 = 323;
    public const int Keypad4 = 324;
    public const int Keypad5 = 325;
    public const int Keypad6 = 326;
    public const int Keypad7 = 327;
    public const int Keypad8 = 328;
    public const int Keypad9 = 329;

    // Modifiers
    public const int LeftShift = 340;
    public const int LeftControl = 341;
    public const int LeftAlt = 342;
    public const int LeftSuper = 343;
    public const int RightShift = 344;
    public const int RightControl = 345;
    public const int RightAlt = 346;
    public const int RightSuper = 347;
}

/// <summary>
/// Mouse button codes.
/// </summary>
public static class MouseButton
{
    public const int Left = 0;
    public const int Right = 1;
    public const int Middle = 2;
}

