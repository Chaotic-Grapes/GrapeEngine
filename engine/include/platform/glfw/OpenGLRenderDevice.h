/* Start Header *****************************************************************/
/*!
\file   OpenGLRenderDevice.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
OpenGL implementation of IRenderDevice interface. Manages OpenGL context
and provides rendering device abstraction.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef OPENGLRENDERDEVICE_H
#define OPENGLRENDERDEVICE_H

#include "platform/IRenderDevice.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

namespace Platform {

    /**
     * @brief OpenGL implementation of IRenderDevice interface
     */
    class OpenGLRenderDevice : public IRenderDevice {
    public:
        OpenGLRenderDevice();
        ~OpenGLRenderDevice() override = default;

        /**
         * @brief Initialize the OpenGL device
         * @param window GLFW window with OpenGL context
         */
        bool Initialize(GLFWwindow* window);

        // ==================== IRenderDevice Implementation ====================
        
        GraphicsAPI GetAPI() const override;
        std::string GetVendor() const override;
        std::string GetRenderer() const override;
        std::string GetVersion() const override;

        void MakeCurrent() override;
        bool IsCurrent() const override;

        void Clear() override;
        void SetClearColor(float r, float g, float b, float a) override;

        void SetViewport(int x, int y, int width, int height) override;
        int GetViewportWidth() const override;
        int GetViewportHeight() const override;

        void SetWireframeMode(bool enabled) override;
        bool IsWireframeMode() const override;

    private:
        GLFWwindow* m_window = nullptr;
        int m_viewportWidth = 0;
        int m_viewportHeight = 0;
        bool m_wireframeMode = false;

        std::string m_vendor;
        std::string m_renderer;
        std::string m_version;
    };

}

#endif
