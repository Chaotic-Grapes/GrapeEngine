/* Start Header *****************************************************************/
/*!
\file   GLFWPlatformContext.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
GLFW-based implementation of IPlatformContext. This is the concrete platform
layer that the engine creates and the editor uses.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GLFWPLATFORMCONTEXT_H
#define GLFWPLATFORMCONTEXT_H

#include "platform/IPlatformContext.h"
#include "GLFWWindow.h"
#include "GLFWInputSystem.h"
#include "OpenGLRenderDevice.h"
#include <vector>
#include <memory>

namespace Platform {

    /**
     * @brief GLFW-based platform context implementation
     * 
     * This class manages GLFW initialization, window creation, and provides
     * access to all platform services through abstract interfaces.
     */
    class GLFWPlatformContext : public IPlatformContext {
    public:
        GLFWPlatformContext();
        ~GLFWPlatformContext() override;

        // ==================== IPlatformContext Implementation ====================

        /**
         * @brief Initialize GLFW and create the default window and OpenGL context.
         * @return True if initialization succeeded.
         */
        bool Initialize() override;

        /**
         * @brief Shut down GLFW and destroy all owned windows and devices.
         */
        void Shutdown() override;

        /**
         * @brief Check whether the platform context has been successfully initialized.
         * @return True if Initialize() completed without error.
         */
        bool IsInitialized() const override;

        /**
         * @brief Create and track a new platform window.
         * @param info Window creation parameters (title, size, mode, etc.).
         * @return Pointer to the new IWindow, or nullptr on failure.
         */
        IWindow* CreatePlatformWindow(const WindowCreateInfo& info) override;

        /**
         * @brief Destroy a previously created window and remove it from the tracked list.
         * @param window Window to destroy.
         */
        void DestroyWindow(IWindow* window) override;

        /**
         * @brief Get the primary application window.
         * @return Pointer to the main IWindow, or nullptr if none exist.
         */
        IWindow* GetMainWindow() const override;

        /**
         * @brief Get all windows currently tracked by this context.
         * @return Reference to the vector of active IWindow pointers.
         */
        const std::vector<IWindow*>& GetAllWindows() const override;

        /**
         * @brief Get the active render device.
         * @return Pointer to the OpenGL render device.
         */
        IRenderDevice* GetRenderDevice() const override;

        /**
         * @brief Get the active input system.
         * @return Pointer to the GLFW input system.
         */
        IInputSystem* GetInputSystem() const override;

        /**
         * @brief Get the platform name string.
         * @return "GLFW" as the platform identifier.
         */
        std::string GetPlatformName() const override;

        /**
         * @brief Get the GLFW version string.
         * @return GLFW version as a human-readable string.
         */
        std::string GetPlatformVersion() const override;

    private:
        bool m_initialized = false;
        std::vector<IWindow*> m_windows;
        std::unique_ptr<GLFWInputSystem> m_inputSystem;
        std::unique_ptr<OpenGLRenderDevice> m_renderDevice;

        /**
         * @brief GLFW error callback registered at initialization.
         * @param error GLFW error code.
         * @param description Human-readable error description string.
         */
        static void _glfwErrorCallback(int error, const char* description);
    };

}

#endif
