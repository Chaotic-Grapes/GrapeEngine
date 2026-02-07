/* Start Header *****************************************************************/
/*!
\file   GLFWWindow.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
GLFW-backed implementation of IWindow interface. Wraps the existing Window
class functionality but exposes it through the platform-agnostic interface.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GLFWWINDOW_H
#define GLFWWINDOW_H

#include "platform/IWindow.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

namespace Platform {

    /**
     * @brief GLFW implementation of IWindow interface
     * 
     * This class wraps GLFW window functionality and exposes it through
     * the platform-agnostic IWindow interface. It maintains backward
     * compatibility with the existing Window class while providing a
     * clean abstraction for the editor.
     */
    class GLFWWindow : public IWindow {
    public:
        GLFWWindow() = default;
        ~GLFWWindow() override;

        // Factory method for creating GLFW windows
        static GLFWWindow* Create(const std::string& title, int width, int height,
                                  bool vsync, WindowMode mode, bool resizable, bool decorated);

        // ==================== IWindow Implementation ====================
        
        void PollEvents() const override;
        void SwapBuffers() const override;
        bool ShouldClose() const override;
        void Close() const override;

        int GetWidth() const override;
        int GetHeight() const override;
        std::string GetTitle() const override;
        void SetTitle(const std::string& title) override;

        bool IsFocused() const override;
        bool IsMinimized() const override;
        void SetMinimized(bool minimized) const override;
        bool IsMaximized() const override;
        void SetMaximized(bool maximized) const override;
        bool IsVisible() const override;
        void SetVisible(bool visible) const override;
        bool IsResizable() const override;
        void SetResizable(bool resizable) const override;

        bool IsVSync() const override;
        void SetVSync(bool enabled) override;

        void SetMode(WindowMode mode) override;
        bool HasMode(WindowMode mode) const override;
        void Resize(int width, int height) override;

        // Display mode queries
        std::vector<Engine::DisplayMode> GetSupportedDisplayModes() const override;
        Engine::MonitorInfo GetMonitorInfo() const override;
        bool SetDisplayMode(const Engine::DisplayMode& mode) override;
        bool SetFullscreen(bool fullscreen) override;

        void* GetNativeHandle() const override;

        // GLFW-specific fullscreen selection
        bool SetFullscreenOnMonitor(int monitorIndex);

        // ==================== GLFW-Specific Methods ====================
        
        /**
         * @brief Get the GLFW window handle
         * @return GLFWwindow pointer
         */
        GLFWwindow* GetGLFWHandle() const { return m_windowHandle; }

        /**
         * @brief Initialize the window (called by factory)
         */
        bool Initialize(const std::string& title, int width, int height,
                       bool vsync, WindowMode mode, bool resizable, bool decorated);

        /**
         * @brief Destroy the window and free resources
         */
        void Destroy();

    private:
        GLFWmonitor* _getCurrentMonitor() const;
        void _storeWindowedPlacement();
        void _restoreWindowedPlacement();
        bool _setBorderless(bool borderless);

        GLFWwindow* m_windowHandle = nullptr;
        std::string m_title;
        int m_width = 0;
        int m_height = 0;
        bool m_vsync = true;
        bool m_resizable = true;
        bool m_decorated = true;
        WindowMode m_mode = WindowMode::Windowed;

        int m_windowedX = 0;
        int m_windowedY = 0;
        int m_windowedWidth = 0;
        int m_windowedHeight = 0;
        bool m_windowedValid = false;
        int m_currentMonitorIndex = 0;

        void _setupCallbacks();
    };

}

#endif
