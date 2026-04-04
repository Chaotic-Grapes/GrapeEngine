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

        /**
         * @brief Factory method for creating a GLFW-backed window.
         * @param title Window title string.
         * @param width Initial window width in pixels.
         * @param height Initial window height in pixels.
         * @param vsync True to enable vertical synchronization.
         * @param mode Initial window mode (Windowed, Fullscreen, or Borderless).
         * @param resizable True to allow the user to resize the window.
         * @param decorated True to show OS window decorations (title bar, borders).
         * @return Pointer to the new GLFWWindow, or nullptr on failure.
         */
        static GLFWWindow* Create(const std::string& title, int width, int height,
                                  bool vsync, WindowMode mode, bool resizable, bool decorated);

        // ==================== IWindow Implementation ====================

        /** @brief Process pending GLFW window and input events. */
        void PollEvents() const override;

        /** @brief Swap the front and back OpenGL buffers to present the frame. */
        void SwapBuffers() const override;

        /**
         * @brief Check whether the user has requested the window to close.
         * @return True if the window close flag is set.
         */
        bool ShouldClose() const override;

        /** @brief Set the window close flag so the main loop will exit. */
        void Close() const override;

        /**
         * @brief Get the window width in pixels.
         * @return Current window width in pixels.
         */
        int GetWidth() const override;

        /**
         * @brief Get the window height in pixels.
         * @return Current window height in pixels.
         */
        int GetHeight() const override;

        /**
         * @brief Get the window title string.
         * @return Current window title.
         */
        std::string GetTitle() const override;

        /**
         * @brief Set the window title string.
         * @param title New title to display in the window title bar.
         */
        void SetTitle(const std::string& title) override;

        /**
         * @brief Check whether the window currently has input focus.
         * @return True if the window is focused.
         */
        bool IsFocused() const override;

        /**
         * @brief Check whether the window is minimized (iconified).
         * @return True if the window is minimized.
         */
        bool IsMinimized() const override;

        /**
         * @brief Minimize or restore the window.
         * @param minimized True to minimize, false to restore.
         */
        void SetMinimized(bool minimized) const override;

        /**
         * @brief Check whether the window is maximized.
         * @return True if the window is maximized.
         */
        bool IsMaximized() const override;

        /**
         * @brief Maximize or restore the window.
         * @param maximized True to maximize, false to restore.
         */
        void SetMaximized(bool maximized) const override;

        /**
         * @brief Check whether the window is currently visible.
         * @return True if the window is visible.
         */
        bool IsVisible() const override;

        /**
         * @brief Show or hide the window.
         * @param visible True to show the window, false to hide it.
         */
        void SetVisible(bool visible) const override;

        /**
         * @brief Check whether the window can be resized by the user.
         * @return True if the window is resizable.
         */
        bool IsResizable() const override;

        /**
         * @brief Enable or disable user resizing of the window.
         * @param resizable True to allow resizing, false to prevent it.
         */
        void SetResizable(bool resizable) const override;

        /**
         * @brief Check whether vertical synchronization is enabled.
         * @return True if VSync is enabled.
         */
        bool IsVSync() const override;

        /**
         * @brief Enable or disable vertical synchronization.
         * @param enabled True to enable VSync, false to disable it.
         */
        void SetVSync(bool enabled) override;

        /**
         * @brief Switch the window to the given display mode.
         * @param mode Target window mode (Windowed, Fullscreen, or Borderless).
         */
        void SetMode(WindowMode mode) override;

        /**
         * @brief Check whether the window is currently in the given mode.
         * @param mode Window mode to test.
         * @return True if the window is in the specified mode.
         */
        bool HasMode(WindowMode mode) const override;

        /**
         * @brief Resize the window to the given dimensions.
         * @param width New width in pixels.
         * @param height New height in pixels.
         */
        void Resize(int width, int height) override;

        /**
         * @brief Get all display modes supported by the current monitor.
         * @return Vector of supported DisplayMode structs.
         */
        std::vector<Engine::DisplayMode> GetSupportedDisplayModes() const override;

        /**
         * @brief Get information about the monitor the window currently occupies.
         * @return MonitorInfo for the current monitor.
         */
        Engine::MonitorInfo GetMonitorInfo() const override;

        /**
         * @brief Apply a specific display mode to the window.
         * @param mode Display mode to set (resolution and refresh rate).
         * @return True if the mode was applied successfully.
         */
        bool SetDisplayMode(const Engine::DisplayMode& mode) override;

        /**
         * @brief Switch the window between windowed and fullscreen.
         * @param fullscreen True to go fullscreen, false to go windowed.
         * @return True if the transition succeeded.
         */
        bool SetFullscreen(bool fullscreen) override;

        /**
         * @brief Get the underlying OS window handle.
         * @return Pointer to the native GLFW window handle.
         */
        void* GetNativeHandle() const override;

        /**
         * @brief Switch to fullscreen mode on a specific monitor by index.
         * @param monitorIndex Zero-based index of the target monitor.
         * @return True if the switch succeeded.
         */
        bool SetFullscreenOnMonitor(int monitorIndex);

        // ==================== GLFW-Specific Methods ====================
        
        /**
         * @brief Get the GLFW window handle
         * @return GLFWwindow pointer
         */
        GLFWwindow* GetGLFWHandle() const { return m_windowHandle; }

        /**
         * @brief Initialize the window (called by factory).
         * @param title Window title string.
         * @param width Initial window width in pixels.
         * @param height Initial window height in pixels.
         * @param vsync True to enable vertical synchronization.
         * @param mode Initial window mode (Windowed, Fullscreen, or Borderless).
         * @param resizable True to allow the user to resize the window.
         * @param decorated True to show OS window decorations.
         * @return True if initialization succeeded.
         */
        bool Initialize(const std::string& title, int width, int height,
                       bool vsync, WindowMode mode, bool resizable, bool decorated);

        /**
         * @brief Destroy the window and free resources
         */
        void Destroy();

    private:
        /** @brief Return the GLFW monitor the window currently occupies the most. */
        GLFWmonitor* _getCurrentMonitor() const;

        /** @brief Save the current windowed position and size so it can be restored later. */
        void _storeWindowedPlacement();

        /** @brief Restore the previously saved windowed position and size. */
        void _restoreWindowedPlacement();

        /**
         * @brief Enable or disable OS-level borderless mode for this window.
         * @param borderless True to remove window borders, false to restore them.
         * @return True if the mode change succeeded.
         */
        bool _setBorderless(bool borderless);

        /** @brief Lock a borderless window to the monitor it currently overlaps the most. */
        void _lockBorderlessToMonitor();


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
        int m_windowedWidth = 0;           // Last windowed width for restore.
        int m_windowedHeight = 0;          // Last windowed height for restore.
        bool m_windowedValid = false;      // True once windowed placement is stored.
        int m_currentMonitorIndex = 0;     // Monitor index for fullscreen transitions.
        bool m_borderlessLockInProgress = false; // Prevent re-entrant lock callbacks.
        int m_borderlessLockedMonitorIndex = -1; // Track monitor already snapped.

        /** @brief Register GLFW event callbacks (resize, focus, close, etc.) for this window. */
        void _setupCallbacks();
    };

}

#endif
