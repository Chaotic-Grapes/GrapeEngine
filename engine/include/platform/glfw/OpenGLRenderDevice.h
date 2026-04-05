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
         * @brief Initialize the OpenGL device.
         * @param window GLFW window whose OpenGL context will be used.
         * @return True if initialization succeeded and GPU info was successfully queried.
         */
        bool Initialize(GLFWwindow* window);

        // ==================== IRenderDevice Implementation ====================

        /**
         * @brief Get the graphics API used by this device.
         * @return GraphicsAPI::OpenGL.
         */
        GraphicsAPI GetAPI() const override;

        /**
         * @brief Get the GPU vendor name string.
         * @return Vendor string (e.g. "NVIDIA Corporation").
         */
        std::string GetVendor() const override;

        /**
         * @brief Get the GPU renderer name string.
         * @return Renderer string (e.g. "GeForce RTX 4090").
         */
        std::string GetRenderer() const override;

        /**
         * @brief Get the OpenGL version string.
         * @return OpenGL version string (e.g. "4.6.0").
         */
        std::string GetVersion() const override;

        /**
         * @brief Make this device's OpenGL context current on the calling thread.
         */
        void MakeCurrent() override;

        /**
         * @brief Check whether this device's context is current on the calling thread.
         * @return True if the context is current.
         */
        bool IsCurrent() const override;

        /**
         * @brief Clear the color and depth buffers using the current clear color.
         */
        void Clear() override;

        /**
         * @brief Set the color used to clear the color buffer.
         * @param r Red component in [0, 1].
         * @param g Green component in [0, 1].
         * @param b Blue component in [0, 1].
         * @param a Alpha component in [0, 1].
         */
        void SetClearColor(float r, float g, float b, float a) override;

        /**
         * @brief Set the OpenGL viewport rectangle.
         * @param x Left edge of the viewport in pixels.
         * @param y Bottom edge of the viewport in pixels.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         */
        void SetViewport(int x, int y, int width, int height) override;

        /**
         * @brief Get the current viewport width in pixels.
         * @return Viewport width in pixels.
         */
        int GetViewportWidth() const override;

        /**
         * @brief Get the current viewport height in pixels.
         * @return Viewport height in pixels.
         */
        int GetViewportHeight() const override;

        /**
         * @brief Enable or disable OpenGL wireframe rendering mode.
         * @param enabled True to enable wireframe, false to restore fill mode.
         */
        void SetWireframeMode(bool enabled) override;

        /**
         * @brief Check whether wireframe rendering mode is active.
         * @return True if wireframe mode is enabled.
         */
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
