/* Start Header *****************************************************************/
/*!
\file   GLFWInputSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GLFW-backed input system abstraction.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "platform/glfw/GLFWInputSystem.h"
#include "services/Input.h"

namespace Platform {

    GLFWInputSystem::GLFWInputSystem() = default;

    /**
     * @brief Binds the GLFW window handle used for cursor mode queries.
     * @param window Pointer to the GLFW window to associate with this input system.
     */
    void GLFWInputSystem::Initialize(GLFWwindow* window) {
        m_window = window;
    }

    // ==================== IInputSystem Implementation ====================

    /**
     * @brief Checks whether the key was just pressed this frame.
     * @param key GLFW key token to query.
     * @return True if the key transitioned from up to down this frame.
     */
    bool GLFWInputSystem::IsKeyPressed(int key) {
        return Input::IsKeyPressed(key);
    }

    /**
     * @brief Checks whether the key is currently held down.
     * @param key GLFW key token to query.
     * @return True if the key is currently held down.
     */
    bool GLFWInputSystem::IsKeyDown(int key) {
        return Input::IsKeyDown(key);
    }

    /**
     * @brief Checks whether the key was just released this frame.
     * @param key GLFW key token to query.
     * @return True if the key transitioned from down to up this frame.
     */
    bool GLFWInputSystem::IsKeyUp(int key) {
        return Input::IsKeyUp(key);
    }

    /**
     * @brief Checks whether the mouse button was just pressed this frame.
     * @param button GLFW mouse button token to query.
     * @return True if the button transitioned from up to down this frame.
     */
    bool GLFWInputSystem::IsMousePressed(int button) {
        return Input::IsMousePressed(button);
    }

    /**
     * @brief Checks whether the mouse button is currently held down.
     * @param button GLFW mouse button token to query.
     * @return True if the button is currently held down.
     */
    bool GLFWInputSystem::IsMouseDown(int button) {
        return Input::IsMouseDown(button);
    }

    /**
     * @brief Checks whether the mouse button was just released this frame.
     * @param button GLFW mouse button token to query.
     * @return True if the button transitioned from down to up this frame.
     */
    bool GLFWInputSystem::IsMouseUp(int button) {
        return Input::IsMouseUp(button);
    }

    /**
     * @brief Retrieves the current mouse cursor position in screen coordinates.
     * @param xPos Output parameter that receives the cursor X position.
     * @param yPos Output parameter that receives the cursor Y position.
     */
    void GLFWInputSystem::GetMousePosition(double& xPos, double& yPos) {
        Input::GetMousePosition(xPos, yPos);
    }

    /**
     * @brief Returns the current horizontal mouse cursor position.
     * @return The cursor X position in screen coordinates.
     */
    double GLFWInputSystem::GetMouseX() {
        return Input::GetMouseX();
    }

    /**
     * @brief Returns the current vertical mouse cursor position.
     * @return The cursor Y position in screen coordinates.
     */
    double GLFWInputSystem::GetMouseY() {
        return Input::GetMouseY();
    }

    /**
     * @brief Returns the horizontal scroll offset accumulated this frame.
     * @return The horizontal scroll delta.
     */
    double GLFWInputSystem::GetScrollX() {
        return Input::GetScrollX();
    }

    /**
     * @brief Returns the vertical scroll offset accumulated this frame.
     * @return The vertical scroll delta.
     */
    double GLFWInputSystem::GetScrollY() {
        return Input::GetScrollY();
    }

    /**
     * @brief Checks whether a gamepad is currently connected.
     * @param gamepad GLFW joystick index to query.
     * @return True if the gamepad is connected and recognized.
     */
    bool GLFWInputSystem::IsGamepadConnected(int gamepad) {
        return Input::IsGamepadConnected(gamepad);
    }

    /**
     * @brief Checks whether a gamepad was connected this frame.
     * @param gamepad GLFW joystick index to query.
     * @return True if the gamepad connected during the current frame.
     */
    bool GLFWInputSystem::IsGamepadJustConnected(int gamepad) {
        return Input::IsGamepadJustConnected(gamepad);
    }

    /**
     * @brief Checks whether a gamepad was disconnected this frame.
     * @param gamepad GLFW joystick index to query.
     * @return True if the gamepad disconnected during the current frame.
     */
    bool GLFWInputSystem::IsGamepadJustDisconnected(int gamepad) {
        return Input::IsGamepadJustDisconnected(gamepad);
    }

    /**
     * @brief Checks whether a gamepad button was just pressed this frame.
     * @param gamepad GLFW joystick index to query.
     * @param button Gamepad button index to query.
     * @return True if the button transitioned from up to down this frame.
     */
    bool GLFWInputSystem::IsGamepadButtonPressed(int gamepad, int button) {
        return Input::IsGamepadButtonPressed(gamepad, button);
    }

    /**
     * @brief Checks whether a gamepad button is currently held down.
     * @param gamepad GLFW joystick index to query.
     * @param button Gamepad button index to query.
     * @return True if the button is currently held down.
     */
    bool GLFWInputSystem::IsGamepadButtonDown(int gamepad, int button) {
        return Input::IsGamepadButtonDown(gamepad, button);
    }

    /**
     * @brief Checks whether a gamepad button was just released this frame.
     * @param gamepad GLFW joystick index to query.
     * @param button Gamepad button index to query.
     * @return True if the button transitioned from down to up this frame.
     */
    bool GLFWInputSystem::IsGamepadButtonUp(int gamepad, int button) {
        return Input::IsGamepadButtonUp(gamepad, button);
    }

    /**
     * @brief Returns the raw axis value for a gamepad axis.
     * @param gamepad GLFW joystick index to query.
     * @param axis Axis index to query.
     * @return Raw axis value in the range [-1.0, 1.0].
     */
    float GLFWInputSystem::GetGamepadAxis(int gamepad, int axis) {
        return Input::GetGamepadAxis(gamepad, axis);
    }

    /**
     * @brief Returns the axis value for a gamepad axis after applying a deadzone.
     * @param gamepad GLFW joystick index to query.
     * @param axis Axis index to query.
     * @param deadzone Magnitude below which the axis value is treated as zero.
     * @return Axis value with the deadzone applied, in the range [-1.0, 1.0].
     */
    float GLFWInputSystem::GetGamepadAxisWithDeadzone(int gamepad, int axis, float deadzone) {
        return Input::GetGamepadAxisWithDeadzone(gamepad, axis, deadzone);
    }

    /**
     * @brief Activates rumble motors on a gamepad at the specified intensities.
     * @param gamepad GLFW joystick index to target.
     * @param lowFrequency Intensity of the low-frequency (heavy) motor in [0.0, 1.0].
     * @param highFrequency Intensity of the high-frequency (light) motor in [0.0, 1.0].
     * @return True if the vibration command was applied successfully.
     */
    bool GLFWInputSystem::SetGamepadVibration(int gamepad, float lowFrequency, float highFrequency) {
        return Input::SetGamepadVibration(gamepad, lowFrequency, highFrequency);
    }

    /**
     * @brief Stops all rumble motor activity on a gamepad.
     * @param gamepad GLFW joystick index to target.
     */
    void GLFWInputSystem::StopGamepadVibration(int gamepad) {
        Input::StopGamepadVibration(gamepad);
    }

    /**
     * @brief Queries whether a gamepad supports vibration feedback.
     * @param gamepad GLFW joystick index to query.
     * @return True if the gamepad exposes vibration capabilities.
     */
    bool GLFWInputSystem::IsGamepadVibrationSupported(int gamepad) {
        return Input::IsGamepadVibrationSupported(gamepad);
    }

    /**
     * @brief Shows or hides the OS cursor over the associated window.
     * @param visible True to show the cursor; false to hide it.
     */
    void GLFWInputSystem::SetCursorVisible(bool visible) {
        if (!m_window) return;

        m_cursorVisible = visible;
        glfwSetInputMode(m_window, GLFW_CURSOR,
                        visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    /**
     * @brief Returns whether the OS cursor is currently visible over the window.
     * @return True if the cursor is visible.
     */
    bool GLFWInputSystem::IsCursorVisible() const {
        return m_cursorVisible;
    }

}
