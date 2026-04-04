/* Start Header *****************************************************************/
/*!
\file   GLFWInputSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
GLFW-backed implementation of IInputSystem interface. Wraps the existing
Input class functionality but exposes it through the platform-agnostic interface.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GLFWINPUTSYSTEM_H
#define GLFWINPUTSYSTEM_H

#include "platform/IInputSystem.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Platform {

    /**
     * @brief GLFW implementation of IInputSystem interface
     * 
    * This class wraps the existing static Input class functionality
     * and exposes it through the platform-agnostic IInputSystem interface.
     */
    class GLFWInputSystem : public IInputSystem {
    public:
        GLFWInputSystem();
        ~GLFWInputSystem() override = default;

        /**
         * @brief Initialize input system with a GLFW window.
         * @param window The GLFW window whose input events this system will process.
         */
        void Initialize(GLFWwindow* window);

        // ==================== IInputSystem Implementation ====================

        /**
         * @brief Check whether a key was just pressed this frame.
         * @param key GLFW key code to query.
         * @return True if the key transitioned to pressed this frame.
         */
        bool IsKeyPressed(int key) override;

        /**
         * @brief Check whether a key is currently held down.
         * @param key GLFW key code to query.
         * @return True if the key is currently held.
         */
        bool IsKeyDown(int key) override;

        /**
         * @brief Check whether a key was just released this frame.
         * @param key GLFW key code to query.
         * @return True if the key transitioned to released this frame.
         */
        bool IsKeyUp(int key) override;

        /**
         * @brief Check whether a mouse button was just pressed this frame.
         * @param button GLFW mouse button code to query.
         * @return True if the button transitioned to pressed this frame.
         */
        bool IsMousePressed(int button) override;

        /**
         * @brief Check whether a mouse button is currently held down.
         * @param button GLFW mouse button code to query.
         * @return True if the button is currently held.
         */
        bool IsMouseDown(int button) override;

        /**
         * @brief Check whether a mouse button was just released this frame.
         * @param button GLFW mouse button code to query.
         * @return True if the button transitioned to released this frame.
         */
        bool IsMouseUp(int button) override;

        /**
         * @brief Get the current cursor position in window coordinates.
         * @param xPos Output receiving the cursor x position.
         * @param yPos Output receiving the cursor y position.
         */
        void GetMousePosition(double& xPos, double& yPos) override;

        /**
         * @brief Get the cursor x position in window coordinates.
         * @return Cursor x coordinate.
         */
        double GetMouseX() override;

        /**
         * @brief Get the cursor y position in window coordinates.
         * @return Cursor y coordinate.
         */
        double GetMouseY() override;

        /**
         * @brief Get the horizontal scroll offset accumulated this frame.
         * @return Horizontal scroll delta.
         */
        double GetScrollX() override;

        /**
         * @brief Get the vertical scroll offset accumulated this frame.
         * @return Vertical scroll delta.
         */
        double GetScrollY() override;

        /**
         * @brief Check whether a gamepad is currently connected.
         * @param gamepad Gamepad index to query.
         * @return True if the gamepad is connected.
         */
        bool IsGamepadConnected(int gamepad) override;

        /**
         * @brief Check whether a gamepad was just connected this frame.
         * @param gamepad Gamepad index to query.
         * @return True if the gamepad connected this frame.
         */
        bool IsGamepadJustConnected(int gamepad) override;

        /**
         * @brief Check whether a gamepad was just disconnected this frame.
         * @param gamepad Gamepad index to query.
         * @return True if the gamepad disconnected this frame.
         */
        bool IsGamepadJustDisconnected(int gamepad) override;

        /**
         * @brief Check whether a gamepad button was just pressed this frame.
         * @param gamepad Gamepad index to query.
         * @param button Button index to query.
         * @return True if the button transitioned to pressed this frame.
         */
        bool IsGamepadButtonPressed(int gamepad, int button) override;

        /**
         * @brief Check whether a gamepad button is currently held down.
         * @param gamepad Gamepad index to query.
         * @param button Button index to query.
         * @return True if the button is currently held.
         */
        bool IsGamepadButtonDown(int gamepad, int button) override;

        /**
         * @brief Check whether a gamepad button was just released this frame.
         * @param gamepad Gamepad index to query.
         * @param button Button index to query.
         * @return True if the button transitioned to released this frame.
         */
        bool IsGamepadButtonUp(int gamepad, int button) override;

        /**
         * @brief Get the current value of a gamepad axis.
         * @param gamepad Gamepad index to query.
         * @param axis Axis index to query.
         * @return Axis value in the range [-1, 1].
         */
        float GetGamepadAxis(int gamepad, int axis) override;

        /**
         * @brief Get the current value of a gamepad axis with a deadzone applied.
         * @param gamepad Gamepad index to query.
         * @param axis Axis index to query.
         * @param deadzone Values within this magnitude are clamped to zero.
         * @return Axis value in the range [-1, 1] with deadzone applied.
         */
        float GetGamepadAxisWithDeadzone(int gamepad, int axis, float deadzone) override;

        /**
         * @brief Set the rumble motors on a gamepad.
         * @param gamepad Gamepad index to vibrate.
         * @param lowFrequency Low-frequency motor intensity in [0, 1].
         * @param highFrequency High-frequency motor intensity in [0, 1].
         * @return True if vibration was set successfully.
         */
        bool SetGamepadVibration(int gamepad, float lowFrequency, float highFrequency) override;

        /**
         * @brief Stop all vibration on a gamepad.
         * @param gamepad Gamepad index to stop.
         */
        void StopGamepadVibration(int gamepad) override;

        /**
         * @brief Check whether a gamepad supports vibration.
         * @param gamepad Gamepad index to query.
         * @return True if vibration is supported.
         */
        bool IsGamepadVibrationSupported(int gamepad) override;

        /**
         * @brief Show or hide the OS cursor.
         * @param visible True to show the cursor, false to hide it.
         */
        void SetCursorVisible(bool visible) override;

        /**
         * @brief Check whether the OS cursor is currently visible.
         * @return True if the cursor is visible.
         */
        bool IsCursorVisible() const override;

    private:
        GLFWwindow* m_window = nullptr;
        bool m_cursorVisible = true;
    };

}

#endif
