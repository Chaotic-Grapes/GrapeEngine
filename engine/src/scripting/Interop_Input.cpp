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

/**
 * @brief Check if a key is currently pressed (true every frame while held)
 *
 * @param key The key code to query
 * @return true if the key is currently pressed, false otherwise
 */
INTEROP_API bool EngineInterop_Input_IsKeyPressed(int key) {
    return Input::IsKeyPressed(key);
}

/**
 * @brief Check if a key was pressed this frame (true only on first frame)
 *
 * @param key The key code to query
 * @return true if the key was pressed this frame, false otherwise
 */
INTEROP_API bool EngineInterop_Input_IsKeyDown(int key) {
    return Input::IsKeyDown(key);
}

/**
 * @brief Check if a key was released this frame
 *
 * @param key The key code to query
 * @return true if the key was released this frame, false otherwise
 */
INTEROP_API bool EngineInterop_Input_IsKeyUp(int key) {
    return Input::IsKeyUp(key);
}

// ============================================================================
// Input API - Mouse
// ============================================================================

/**
 * @brief Check if a mouse button is currently pressed
 *
 * @param button The mouse button index to query
 * @return true if the button is currently pressed, false otherwise
 */
INTEROP_API bool EngineInterop_Input_IsMousePressed(int button) {
    return Input::IsMousePressed(button);
}

/**
 * @brief Get the current mouse X position
 *
 * @return double The current mouse X coordinate
 */
INTEROP_API double EngineInterop_Input_GetMouseX() {
    return Input::GetMouseX();
}

/**
 * @brief Get the current mouse Y position
 *
 * @return double The current mouse Y coordinate
 */
INTEROP_API double EngineInterop_Input_GetMouseY() {
    return Input::GetMouseY();
}

/**
 * @brief Get the mouse scroll delta X (horizontal scroll)
 *
 * @return double The horizontal scroll delta
 */
INTEROP_API double EngineInterop_Input_GetScrollX() {
    return Input::GetScrollX();
}

/**
 * @brief Get the mouse scroll delta Y (vertical scroll)
 *
 * @return double The vertical scroll delta
 */
INTEROP_API double EngineInterop_Input_GetScrollY() {
    return Input::GetScrollY();
}

// ============================================================================
// Input API - Gamepad
// ============================================================================

/**
 * @brief Check if a gamepad slot is currently connected
 *
 * @param gamepad The gamepad index (0..15)
 * @return true if connected as a mapped gamepad, false otherwise
 */
INTEROP_API bool EngineInterop_Input_IsGamepadConnected(int gamepad) {
    return Input::IsGamepadConnected(gamepad);
}

/**
 * @brief Check if a gamepad was connected this frame
 *
 * @param gamepad The gamepad index (0..15)
 * @return true if the gamepad transitioned to connected this frame
 */
INTEROP_API bool EngineInterop_Input_IsGamepadJustConnected(int gamepad) {
    return Input::IsGamepadJustConnected(gamepad);
}

/**
 * @brief Check if a gamepad was disconnected this frame
 *
 * @param gamepad The gamepad index (0..15)
 * @return true if the gamepad transitioned to disconnected this frame
 */
INTEROP_API bool EngineInterop_Input_IsGamepadJustDisconnected(int gamepad) {
    return Input::IsGamepadJustDisconnected(gamepad);
}

/**
 * @brief Check if a gamepad button was pressed this frame
 *
 * @param gamepad The gamepad index (0..15)
 * @param button The gamepad button index
 * @return true if pressed this frame
 */
INTEROP_API bool EngineInterop_Input_IsGamepadButtonPressed(int gamepad, int button) {
    return Input::IsGamepadButtonPressed(gamepad, button);
}

/**
 * @brief Check if a gamepad button is currently held
 *
 * @param gamepad The gamepad index (0..15)
 * @param button The gamepad button index
 * @return true if currently down
 */
INTEROP_API bool EngineInterop_Input_IsGamepadButtonDown(int gamepad, int button) {
    return Input::IsGamepadButtonDown(gamepad, button);
}

/**
 * @brief Check if a gamepad button was released this frame
 *
 * @param gamepad The gamepad index (0..15)
 * @param button The gamepad button index
 * @return true if released this frame
 */
INTEROP_API bool EngineInterop_Input_IsGamepadButtonUp(int gamepad, int button) {
    return Input::IsGamepadButtonUp(gamepad, button);
}

/**
 * @brief Get raw gamepad axis value in range [-1, 1]
 *
 * @param gamepad The gamepad index (0..15)
 * @param axis The gamepad axis index
 * @return float Axis value
 */
INTEROP_API float EngineInterop_Input_GetGamepadAxis(int gamepad, int axis) {
    return Input::GetGamepadAxis(gamepad, axis);
}

/**
 * @brief Get gamepad axis value with deadzone filtering
 *
 * @param gamepad The gamepad index (0..15)
 * @param axis The gamepad axis index
 * @param deadzone Deadzone in [0, 1)
 * @return float Filtered axis value
 */
INTEROP_API float EngineInterop_Input_GetGamepadAxisWithDeadzone(int gamepad, int axis, float deadzone) {
    return Input::GetGamepadAxisWithDeadzone(gamepad, axis, deadzone);
}
