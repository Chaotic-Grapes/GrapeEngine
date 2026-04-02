/* Start Header *****************************************************************/
/*!
\file   IInputSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Platform abstraction interface for input system. Provides a clean API
for keyboard, mouse, and gamepad input without exposing underlying platform
implementation (e.g., GLFW, Win32).

This allows the editor to handle input without directly depending on
platform-specific input handling code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef IINPUTSYSTEM_H
#define IINPUTSYSTEM_H

#include "Export.h"

namespace Platform {

    /**
     * @brief Platform-agnostic input system interface
     * 
    * This interface abstracts keyboard, mouse, and gamepad input handling, allowing
     * the editor and engine to query input state without knowing the
     * specific platform implementation.
     */
    class GRAPEENGINE_API IInputSystem {
    public:
        virtual ~IInputSystem() = default;

        // ==================== Keyboard Input ====================
        
        /**
         * @brief Check if a key was just pressed this frame
         * @param key Key code (platform-specific, but typically matches GLFW)
         */
        virtual bool IsKeyPressed(int key) = 0;

        /**
         * @brief Check if a key is currently held down
         * @param key Key code
         */
        virtual bool IsKeyDown(int key) = 0;

        /**
         * @brief Check if a key was just released this frame
         * @param key Key code
         */
        virtual bool IsKeyUp(int key) = 0;

        // ==================== Mouse Input ====================
        
        /**
         * @brief Check if a mouse button was just pressed this frame
         * @param button Mouse button code (0=left, 1=right, 2=middle)
         */
        virtual bool IsMousePressed(int button) = 0;

        /**
         * @brief Check if a mouse button is currently held down
         * @param button Mouse button code
         */
        virtual bool IsMouseDown(int button) = 0;

        /**
         * @brief Check if a mouse button was just released this frame
         * @param button Mouse button code
         */
        virtual bool IsMouseUp(int button) = 0;

        /**
         * @brief Get current mouse cursor position
         * @param xPos Output: X coordinate in window space
         * @param yPos Output: Y coordinate in window space
         */
        virtual void GetMousePosition(double& xPos, double& yPos) = 0;

        /**
         * @brief Get mouse X coordinate
         */
        virtual double GetMouseX() = 0;

        /**
         * @brief Get mouse Y coordinate
         */
        virtual double GetMouseY() = 0;

        /**
         * @brief Get horizontal scroll offset this frame
         */
        virtual double GetScrollX() = 0;

        /**
         * @brief Get vertical scroll offset this frame
         */
        virtual double GetScrollY() = 0;

        // ==================== Gamepad Input ====================

        /**
         * @brief Check whether a gamepad slot is currently connected
         * @param gamepad Gamepad index (0..15, maps to GLFW joystick ids)
         */
        virtual bool IsGamepadConnected(int gamepad) = 0;

        /**
         * @brief Check if a gamepad was connected this frame
         * @param gamepad Gamepad index (0..15)
         */
        virtual bool IsGamepadJustConnected(int gamepad) = 0;

        /**
         * @brief Check if a gamepad was disconnected this frame
         * @param gamepad Gamepad index (0..15)
         */
        virtual bool IsGamepadJustDisconnected(int gamepad) = 0;

        /**
         * @brief Check if a gamepad button transitioned from up to down this frame
         * @param gamepad Gamepad index (0..15)
         * @param button Gamepad button index (GLFW gamepad button enum)
         */
        virtual bool IsGamepadButtonPressed(int gamepad, int button) = 0;

        /**
         * @brief Check if a gamepad button is currently held
         * @param gamepad Gamepad index (0..15)
         * @param button Gamepad button index (GLFW gamepad button enum)
         */
        virtual bool IsGamepadButtonDown(int gamepad, int button) = 0;

        /**
         * @brief Check if a gamepad button transitioned from down to up this frame
         * @param gamepad Gamepad index (0..15)
         * @param button Gamepad button index (GLFW gamepad button enum)
         */
        virtual bool IsGamepadButtonUp(int gamepad, int button) = 0;

        /**
         * @brief Get raw gamepad axis value in range [-1, 1]
         * @param gamepad Gamepad index (0..15)
         * @param axis Gamepad axis index (GLFW gamepad axis enum)
         */
        virtual float GetGamepadAxis(int gamepad, int axis) = 0;

        /**
         * @brief Get gamepad axis value after radial deadzone filtering
         * @param gamepad Gamepad index (0..15)
         * @param axis Gamepad axis index (GLFW gamepad axis enum)
         * @param deadzone Deadzone in [0, 1)
         */
        virtual float GetGamepadAxisWithDeadzone(int gamepad, int axis, float deadzone) = 0;

        /**
         * @brief Set gamepad vibration motor intensities
         * @param gamepad Gamepad index (0..15)
         * @param lowFrequency Low-frequency motor intensity in [0, 1]
         * @param highFrequency High-frequency motor intensity in [0, 1]
         * @return True when vibration command was accepted by backend
         */
        virtual bool SetGamepadVibration(int gamepad, float lowFrequency, float highFrequency) = 0;

        /**
         * @brief Stop vibration for a gamepad slot
         * @param gamepad Gamepad index (0..15)
         */
        virtual void StopGamepadVibration(int gamepad) = 0;

        /**
         * @brief Query whether vibration is available for a gamepad slot
         * @param gamepad Gamepad index (0..15)
         */
        virtual bool IsGamepadVibrationSupported(int gamepad) = 0;

        // ==================== Cursor Management ====================
        
        /**
         * @brief Set cursor visibility
         */
        virtual void SetCursorVisible(bool visible) = 0;

        /**
         * @brief Check if cursor is visible
         */
        virtual bool IsCursorVisible() const = 0;
    };

}

#endif
