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

    void GLFWInputSystem::Initialize(GLFWwindow* window) {
        m_window = window;
    }

    // ==================== IInputSystem Implementation ====================

    bool GLFWInputSystem::IsKeyPressed(int key) {
        return Input::IsKeyPressed(key);
    }

    bool GLFWInputSystem::IsKeyDown(int key) {
        return Input::IsKeyDown(key);
    }

    bool GLFWInputSystem::IsKeyUp(int key) {
        return Input::IsKeyUp(key);
    }

    bool GLFWInputSystem::IsMousePressed(int button) {
        return Input::IsMousePressed(button);
    }

    bool GLFWInputSystem::IsMouseDown(int button) {
        return Input::IsMouseDown(button);
    }

    bool GLFWInputSystem::IsMouseUp(int button) {
        return Input::IsMouseUp(button);
    }

    void GLFWInputSystem::GetMousePosition(double& xPos, double& yPos) {
        Input::GetMousePosition(xPos, yPos);
    }

    double GLFWInputSystem::GetMouseX() {
        return Input::GetMouseX();
    }

    double GLFWInputSystem::GetMouseY() {
        return Input::GetMouseY();
    }

    double GLFWInputSystem::GetScrollX() {
        return Input::GetScrollX();
    }

    double GLFWInputSystem::GetScrollY() {
        return Input::GetScrollY();
    }

    void GLFWInputSystem::SetCursorVisible(bool visible) {
        if (!m_window) return;
        
        m_cursorVisible = visible;
        glfwSetInputMode(m_window, GLFW_CURSOR, 
                        visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    bool GLFWInputSystem::IsCursorVisible() const {
        return m_cursorVisible;
    }

}
