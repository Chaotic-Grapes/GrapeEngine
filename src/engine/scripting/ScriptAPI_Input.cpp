/* Start Header *****************************************************************/
/*!
\file   ScriptAPI_Input.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
C# scripting API exports for input handling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/Input.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// ============================================================================
// Input API - Keyboard
// ============================================================================

/// <summary>
/// Check if a key is currently pressed (true every frame while held)
/// </summary>
SCRIPT_API bool ScriptAPI_IsKeyPressed(int key) {
    return Input::IsKeyPressed(key);
}

/// <summary>
/// Check if a key was pressed this frame (true only on first frame)
/// </summary>
SCRIPT_API bool ScriptAPI_IsKeyDown(int key) {
    return Input::IsKeyDown(key);
}

/// <summary>
/// Check if a key was released this frame
/// </summary>
SCRIPT_API bool ScriptAPI_IsKeyUp(int key) {
    return Input::IsKeyUp(key);
}

// ============================================================================
// Input API - Mouse
// ============================================================================

/// <summary>
/// Check if a mouse button is currently pressed
/// </summary>
SCRIPT_API bool ScriptAPI_IsMousePressed(int button) {
    return Input::IsMousePressed(button);
}

/// <summary>
/// Get the current mouse X position
/// </summary>
SCRIPT_API double ScriptAPI_GetMouseX() {
    return Input::GetMouseX();
}

/// <summary>
/// Get the current mouse Y position
/// </summary>
SCRIPT_API double ScriptAPI_GetMouseY() {
    return Input::GetMouseY();
}

/// <summary>
/// Get the mouse scroll delta X (horizontal scroll)
/// </summary>
SCRIPT_API double ScriptAPI_GetScrollX() {
    return Input::GetScrollX();
}

/// <summary>
/// Get the mouse scroll delta Y (vertical scroll)
/// </summary>
SCRIPT_API double ScriptAPI_GetScrollY() {
    return Input::GetScrollY();
}

// ============================================================================
// Input API - Window
// ============================================================================

/// <summary>
/// Get the current window width in pixels
/// </summary>
SCRIPT_API int ScriptAPI_GetWindowWidth() {
    return Input::GetWindowWidth();
}

/// <summary>
/// Get the current window height in pixels
/// </summary>
SCRIPT_API int ScriptAPI_GetWindowHeight() {
    return Input::GetWindowHeight();
}
