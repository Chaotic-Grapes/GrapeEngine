/* Start Header *****************************************************************/
/*!
\file   IRenderDevice.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Platform abstraction interface for rendering device. Provides a clean API
for graphics context management without exposing underlying implementation
(e.g., OpenGL, Vulkan).

This allows the editor to interact with rendering without directly depending
on graphics API specifics.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef IRENDERDEVICE_H
#define IRENDERDEVICE_H

#include "Export.h"
#include <string>

namespace Platform {

    /**
     * @brief Graphics API type
     */
    enum class GraphicsAPI {
        OpenGL,
    };

    /**
     * @brief Platform-agnostic rendering device interface
     * 
     * This interface abstracts graphics context management, allowing the
     * editor and engine to interact with rendering without knowing the
     * specific graphics API being used.
     */
    class GRAPEENGINE_API IRenderDevice {
    public:
        virtual ~IRenderDevice() = default;

        // ==================== Device Properties ====================
        
        /**
         * @brief Get the graphics API type
         */
        virtual GraphicsAPI GetAPI() const = 0;

        /**
         * @brief Get GPU vendor name
         */
        virtual std::string GetVendor() const = 0;

        /**
         * @brief Get GPU renderer name
         */
        virtual std::string GetRenderer() const = 0;

        /**
         * @brief Get graphics API version string
         */
        virtual std::string GetVersion() const = 0;

        // ==================== Context Management ====================
        
        /**
         * @brief Make this device's context current for the calling thread
         */
        virtual void MakeCurrent() = 0;

        /**
         * @brief Check if this device's context is current
         */
        virtual bool IsCurrent() const = 0;

        /**
         * @brief Clear color and depth buffers
         */
        virtual void Clear() = 0;

        /**
         * @brief Set clear color (RGBA, 0-1 range)
         */
        virtual void SetClearColor(float r, float g, float b, float a) = 0;

        // ==================== Viewport ====================
        
        /**
         * @brief Set viewport dimensions
         */
        virtual void SetViewport(int x, int y, int width, int height) = 0;

        /**
         * @brief Get current viewport width
         */
        virtual int GetViewportWidth() const = 0;

        /**
         * @brief Get current viewport height
         */
        virtual int GetViewportHeight() const = 0;

        // ==================== Debug ====================
        
        /**
         * @brief Enable or disable wireframe rendering
         */
        virtual void SetWireframeMode(bool enabled) = 0;

        /**
         * @brief Check if wireframe mode is enabled
         */
        virtual bool IsWireframeMode() const = 0;
    };

}

#endif
