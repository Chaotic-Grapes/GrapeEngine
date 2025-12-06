/* Start Header *****************************************************************/
/*!
\file   IPlatformContext.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Platform context interface that provides access to all platform services.
This is the main entry point for accessing window, rendering, and input
services in a platform-agnostic manner.

The engine provides a concrete implementation, and the editor uses this
interface to access all platform functionality without coupling to GLFW
or other platform-specific code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef IPLATFORMCONTEXT_H
#define IPLATFORMCONTEXT_H

#include "Export.h"
#include "IWindow.h"
#include "IRenderDevice.h"
#include "IInputSystem.h"
#include <memory>
#include <string>
#include <vector>

namespace Platform {

    /**
     * @brief Window creation parameters
     */
    struct WindowCreateInfo {
        std::string Title = "Grape Engine";
        int Width = 1280;
        int Height = 720;
        bool VSync = true;
        WindowMode Mode = WindowMode::Windowed;
        bool Resizable = true;
        bool Decorated = true;
    };

    /**
     * @brief Platform context interface - main entry point for platform services
     * 
     * This interface provides access to all platform-specific functionality
     * through abstract interfaces. The engine creates and owns the concrete
     * implementation, and the editor uses this interface to access platform
     * services.
     * 
     * Example usage in editor:
     * ```cpp
     * auto* platform = engine->GetPlatformContext();
     * auto* window = platform->GetMainWindow();
     * window->SetTitle("My Editor");
     * ```
     */
    class GRAPEENGINE_API IPlatformContext {
    public:
        virtual ~IPlatformContext() = default;

        // ==================== Initialization ====================
        
        /**
         * @brief Initialize the platform subsystem
         * @return true if initialization succeeded
         */
        virtual bool Initialize() = 0;

        /**
         * @brief Shutdown the platform subsystem
         */
        virtual void Shutdown() = 0;

        /**
         * @brief Check if platform is initialized
         */
        virtual bool IsInitialized() const = 0;

        // ==================== Window Management ====================
        
        /**
         * @brief Create a new window
         * @param info Window creation parameters
         * @return Pointer to created window interface, or nullptr on failure
         */
        virtual IWindow* CreatePlatformWindow(const WindowCreateInfo& info) = 0;

        /**
         * @brief Destroy a window
         * @param window Window to destroy
         */
        virtual void DestroyWindow(IWindow* window) = 0;

        /**
         * @brief Get the main application window
         * @return Pointer to main window, or nullptr if no window exists
         */
        virtual IWindow* GetMainWindow() const = 0;

        /**
         * @brief Get all created windows
         * @return Array of window pointers
         */
        virtual const std::vector<IWindow*>& GetAllWindows() const = 0;

        // ==================== Service Access ====================
        
        /**
         * @brief Get the render device interface
         */
        virtual IRenderDevice* GetRenderDevice() const = 0;

        /**
         * @brief Get the input system interface
         */
        virtual IInputSystem* GetInputSystem() const = 0;

        // ==================== Platform Info ====================
        
        /**
         * @brief Get platform name (e.g., "GLFW", "Win32")
         */
        virtual std::string GetPlatformName() const = 0;

        /**
         * @brief Get platform version string
         */
        virtual std::string GetPlatformVersion() const = 0;
    };

}

#endif
