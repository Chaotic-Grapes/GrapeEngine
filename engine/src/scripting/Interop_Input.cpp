/* Start Header *****************************************************************/
/*!
\file   Interop_Input.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
C API exports for input handling used by C# scripting systems.
This provides the bridge between C++ input services and managed C# code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "services/Input.h"

// ============================================================================
// Input API - Keyboard
// ============================================================================

/// <summary>
/// Check if a key is currently pressed (true every frame while held)
/// </summary>
INTEROP_API bool EngineInterop_Input_IsKeyPressed(int key) {
    return Input::IsKeyPressed(key);
}

/// <summary>
/// Check if a key was pressed this frame (true only on first frame)
/// </summary>
INTEROP_API bool EngineInterop_Input_IsKeyDown(int key) {
    return Input::IsKeyDown(key);
}

/// <summary>
/// Check if a key was released this frame
/// </summary>
INTEROP_API bool EngineInterop_Input_IsKeyUp(int key) {
    return Input::IsKeyUp(key);
}

// ============================================================================
// Input API - Mouse
// ============================================================================

/// <summary>
/// Check if a mouse button is currently pressed
/// </summary>
INTEROP_API bool EngineInterop_Input_IsMousePressed(int button) {
    return Input::IsMousePressed(button);
}

/// <summary>
/// Get the current mouse X position
/// </summary>
INTEROP_API double EngineInterop_Input_GetMouseX() {
    return Input::GetMouseX();
}

/// <summary>
/// Get the current mouse Y position
/// </summary>
INTEROP_API double EngineInterop_Input_GetMouseY() {
    return Input::GetMouseY();
}

/// <summary>
/// Get the mouse scroll delta X (horizontal scroll)
/// </summary>
INTEROP_API double EngineInterop_Input_GetScrollX() {
    return Input::GetScrollX();
}

/// <summary>
/// Get the mouse scroll delta Y (vertical scroll)
/// </summary>
INTEROP_API double EngineInterop_Input_GetScrollY() {
    return Input::GetScrollY();
}
