/* Start Header *****************************************************************/
/*!
\file   IWindow.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Platform abstraction interface for window management. Provides a clean API
for window operations without exposing underlying platform implementation
(e.g., GLFW, Win32).

This interface allows the editor to use window services without directly
depending on GLFW or platform-specific code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef IWINDOW_H
#define IWINDOW_H

#include "Export.h"
#include <string>
#include <cstdint>
#include <vector>

// Forward declarations for DeviceManager types
namespace Engine {
    struct DisplayMode;
    struct MonitorInfo;
}

namespace Platform {

    /**
     * @brief Window display mode flags
     */
    enum class WindowMode : uint8_t {
        Windowed = 1 << 0,
        Fullscreen = 1 << 1,
        Borderless = 1 << 2
    };
    

    /**
     * @brief Platform-agnostic window interface
     * 
     * This interface abstracts window creation, management, and rendering
     * without exposing the underlying platform implementation. The engine
     * provides concrete implementations (e.g., GLFWWindow), and the editor
     * uses only this interface.
     */
    class GRAPEENGINE_API IWindow {
    public:
        virtual ~IWindow() = default;

        // ==================== Window Lifecycle ====================
        
        /**
         * @brief Poll platform events (input, resize, etc.)
         */
        virtual void PollEvents() const = 0;

        /**
         * @brief Swap front and back buffers
         */
        virtual void SwapBuffers() const = 0;

        /**
         * @brief Check if window should close
         * @return true if close was requested
         */
        virtual bool ShouldClose() const = 0;

        /**
         * @brief Request window to close
         */
        virtual void Close() const = 0;

        // ==================== Window Properties ====================
        
        /**
         * @brief Get window width in pixels
         * @return Window width in pixels.
         */
        virtual int GetWidth() const = 0;

        /**
         * @brief Get window height in pixels
         * @return Window height in pixels.
         */
        virtual int GetHeight() const = 0;

        /**
         * @brief Get window title
         * @return Current window title string.
         */
        virtual std::string GetTitle() const = 0;

        /**
         * @brief Set window title
         * @param title New title string to display in the window title bar.
         */
        virtual void SetTitle(const std::string& title) = 0;

        // ==================== Window State ====================
        
        /**
         * @brief Check if window has input focus
         * @return True if the window currently has keyboard/mouse focus.
         */
        virtual bool IsFocused() const = 0;

        /**
         * @brief Check if window is minimized
         * @return True if the window is currently minimized.
         */
        virtual bool IsMinimized() const = 0;

        /**
         * @brief Set window minimized state
         * @param minimized True to minimize the window, false to restore.
         */
        virtual void SetMinimized(bool minimized) const = 0;

        /**
         * @brief Check if window is maximized
         * @return True if the window is currently maximized.
         */
        virtual bool IsMaximized() const = 0;

        /**
         * @brief Set window maximized state
         * @param maximized True to maximize the window, false to restore.
         */
        virtual void SetMaximized(bool maximized) const = 0;

        /**
         * @brief Check if window is visible
         * @return True if the window is currently shown.
         */
        virtual bool IsVisible() const = 0;

        /**
         * @brief Set window visibility
         * @param visible True to show the window, false to hide it.
         */
        virtual void SetVisible(bool visible) const = 0;

        /**
         * @brief Check if window is resizable
         * @return True if the window can be resized by the user.
         */
        virtual bool IsResizable() const = 0;

        /**
         * @brief Set window resizable state
         * @param resizable True to allow user resizing, false to fix the window size.
         */
        virtual void SetResizable(bool resizable) const = 0;

        // ==================== Rendering ====================
        
        /**
         * @brief Check if VSync is enabled
         * @return True if vertical synchronization is currently active.
         */
        virtual bool IsVSync() const = 0;

        /**
         * @brief Enable or disable VSync
         * @param enabled True to enable vertical synchronization, false to disable.
         */
        virtual void SetVSync(bool enabled) = 0;

        // ==================== Window Mode and Resizing ====================
        
        /**
         * @brief Set window mode (Windowed, Fullscreen, Borderless)
         * @param mode The desired window mode
         * 
         * Note: Not all platforms support all modes. Implementation may
         * fall back to supported modes.
         */
        virtual void SetMode(WindowMode mode) = 0;

        /**
         * @brief Check if window has specific mode flags
         * @param mode Mode flags to check.
         * @return True if all specified mode flags are active.
         */
        virtual bool HasMode(WindowMode mode) const = 0;

        /**
         * @brief Resize window to specified dimensions
         * @param width New width in pixels (0 to keep current)
         * @param height New height in pixels (0 to keep current)
         * 
         * Note: May not work in fullscreen or borderless modes.
         */
        virtual void Resize(int width, int height) = 0;

        // ==================== Display Mode Queries ====================

        /**
         * @brief Get supported display modes for this window's monitor
         * @return Vector of available DisplayMode options
         * 
         * Queries the monitor that this window is currently on and returns
         * all available resolution/refresh rate combinations.
         */
        virtual std::vector<Engine::DisplayMode> GetSupportedDisplayModes() const = 0;

        /**
         * @brief Get information about the monitor this window is on
         * @return MonitorInfo for the current display
         * 
         * Returns detailed information including physical size, DPI, position,
         * and currently active display mode.
         */
        virtual Engine::MonitorInfo GetMonitorInfo() const = 0;

        /**
         * @brief Apply a display mode to this window
         * @param mode The DisplayMode to apply
         * @return true if successfully applied
         * 
         * Attempts to set the window to the specified resolution and refresh rate.
         * Validates that the mode is supported before applying.
         */
        virtual bool SetDisplayMode(const Engine::DisplayMode& mode) = 0;

        /**
         * @brief Set fullscreen state
         * @param fullscreen true for fullscreen, false for windowed
         * @return true if successfully changed
         * 
         * Transitions between fullscreen and windowed modes. In fullscreen mode,
         * the window will use the monitor's native refresh rate.
         */
        virtual bool SetFullscreen(bool fullscreen) = 0;

        // ==================== Platform-Specific Handle ====================
        
        /**
         * @brief Get native window handle (platform-dependent)
         * @return Opaque pointer to native window (e.g., GLFWwindow*)
         * 
         * This should only be used when interfacing with platform-specific
         * libraries (e.g., ImGui GLFW backend). Avoid using this in
         * cross-platform code.
         */
        virtual void* GetNativeHandle() const = 0;
    };

    /**
     * @brief Combine two WindowMode flags with bitwise OR.
     * @param a First mode flags.
     * @param b Second mode flags.
     * @return Union of the two flag sets.
     */
    inline WindowMode operator|(WindowMode a, WindowMode b) {
        return static_cast<WindowMode>(
            static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
        );
    }

    /**
     * @brief Intersect two WindowMode flags with bitwise AND.
     * @param a First mode flags.
     * @param b Second mode flags.
     * @return Intersection of the two flag sets.
     */
    inline WindowMode operator&(WindowMode a, WindowMode b) {
        return static_cast<WindowMode>(
            static_cast<uint8_t>(a) & static_cast<uint8_t>(b)
        );
    }

    /**
     * @brief Test whether a specific WindowMode flag is set.
     * @param flags Combined flags to test.
     * @param flag Single flag to check for.
     * @return True if the flag is present in flags.
     */
    inline bool HasFlag(WindowMode flags, WindowMode flag) {
        return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
    }

}

#endif
