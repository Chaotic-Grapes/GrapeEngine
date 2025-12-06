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
         * @brief Initialize input system with a GLFW window
         */
        void Initialize(GLFWwindow* window);

        // ==================== IInputSystem Implementation ====================
        
        bool IsKeyPressed(int key) override;
        bool IsKeyDown(int key) override;
        bool IsKeyUp(int key) override;

        bool IsMousePressed(int button) override;
        bool IsMouseDown(int button) override;
        bool IsMouseUp(int button) override;

        void GetMousePosition(double& xPos, double& yPos) override;
        double GetMouseX() override;
        double GetMouseY() override;

        double GetScrollX() override;
        double GetScrollY() override;

        void SetCursorVisible(bool visible) override;
        bool IsCursorVisible() const override;

    private:
        GLFWwindow* m_window = nullptr;
        bool m_cursorVisible = true;
    };

}

#endif
