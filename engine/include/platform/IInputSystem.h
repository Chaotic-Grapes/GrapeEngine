/* Start Header *****************************************************************/
/*!
\file   IInputSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Platform abstraction interface for input system. Provides a clean API
for keyboard and mouse input without exposing underlying platform
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
     * This interface abstracts keyboard and mouse input handling, allowing
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
