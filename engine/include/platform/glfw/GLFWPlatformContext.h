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
        
        bool Initialize() override;
        void Shutdown() override;
        bool IsInitialized() const override;

        IWindow* CreatePlatformWindow(const WindowCreateInfo& info) override;
        void DestroyWindow(IWindow* window) override;
        IWindow* GetMainWindow() const override;
        const std::vector<IWindow*>& GetAllWindows() const override;

        IRenderDevice* GetRenderDevice() const override;
        IInputSystem* GetInputSystem() const override;

        std::string GetPlatformName() const override;
        std::string GetPlatformVersion() const override;

    private:
        bool m_initialized = false;
        std::vector<IWindow*> m_windows;
        std::unique_ptr<GLFWInputSystem> m_inputSystem;
        std::unique_ptr<OpenGLRenderDevice> m_renderDevice;

        static void _glfwErrorCallback(int error, const char* description);
    };

}

#endif
