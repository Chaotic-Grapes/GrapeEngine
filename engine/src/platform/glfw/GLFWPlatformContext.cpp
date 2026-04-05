/* Start Header *****************************************************************/
/*!
\file   GLFWPlatformContext.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GLFW-based platform context.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "platform/glfw/GLFWPlatformContext.h"
#include "core/Logger.h"

namespace Platform {

    GLFWPlatformContext::GLFWPlatformContext() = default;

    GLFWPlatformContext::~GLFWPlatformContext() {
        Shutdown();
    }

    /**
     * @brief GLFW error callback that forwards error details to the engine logger.
     * @param error GLFW numeric error code.
     * @param description Human-readable description of the error.
     */
    void GLFWPlatformContext::_glfwErrorCallback(int error, const char* description) {
        LOG_ERROR("GLFW Error [" << error << "]: " << description);
    }

    /**
     * @brief Initializes GLFW, creates the input system, and prepares the render device.
     * @return True if initialization succeeded; false on any GLFW startup failure.
     */
    bool GLFWPlatformContext::Initialize() {
        if (m_initialized) {
            LOG_WARNING("GLFWPlatformContext already initialized");
            return true;
        }

        LOG_INFO("Initializing GLFW Platform Context...");

        // Set error callback
        glfwSetErrorCallback(_glfwErrorCallback);

        // Initialize GLFW
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            return false;
        }

        LOG_INFO("GLFW initialized successfully");
        LOG_INFO("GLFW Version: " << glfwGetVersionString());

        // Create input system
        m_inputSystem = std::make_unique<GLFWInputSystem>();

        // Create render device (will be initialized when first window is created)
        m_renderDevice = std::make_unique<OpenGLRenderDevice>();

        m_initialized = true;
        return true;
    }

    /**
     * @brief Destroys all managed windows, releases subsystems, and terminates GLFW.
     */
    void GLFWPlatformContext::Shutdown() {
        if (!m_initialized) return;

        LOG_INFO("Shutting down GLFW Platform Context...");

        // Destroy all windows
        for (auto* window : m_windows) {
            if (auto* glfwWindow = dynamic_cast<GLFWWindow*>(window)) {
                glfwWindow->Destroy();
            }
            delete window;
        }
        m_windows.clear();

        // Cleanup subsystems
        m_renderDevice.reset();
        m_inputSystem.reset();

        // Terminate GLFW
        glfwTerminate();

        m_initialized = false;
        LOG_INFO("GLFW Platform Context shutdown complete");
    }

    /**
     * @brief Returns whether the platform context has been successfully initialized.
     * @return True if Initialize() has completed without error.
     */
    bool GLFWPlatformContext::IsInitialized() const {
        return m_initialized;
    }

    /**
     * @brief Creates a new platform window and, for the first window, initializes the
     *        render device and input system against it.
     * @param info Descriptor containing title, dimensions, mode, and other window properties.
     * @return Pointer to the newly created IWindow, or nullptr on failure.
     */
    IWindow* GLFWPlatformContext::CreatePlatformWindow(const WindowCreateInfo& info) {
        if (!m_initialized) {
            LOG_ERROR("Cannot create window: platform not initialized");
            return nullptr;
        }

        // Map platform-agnostic WindowMode to implementation
        WindowMode mode = info.Mode;

        // Create GLFW window
        auto* window = GLFWWindow::Create(info.Title, info.Width, info.Height,
                                          info.VSync, mode, info.Resizable, info.Decorated);
        if (!window) {
            LOG_ERROR("Failed to create GLFW window");
            return nullptr;
        }

        // Initialize render device with first window
        if (m_windows.empty() && m_renderDevice) {
            if (!m_renderDevice->Initialize(window->GetGLFWHandle())) {
                LOG_ERROR("Render device initialization failed for window: " << info.Title);
            }
        }

        // Initialize input system with first window
        if (m_windows.empty() && m_inputSystem) {
            m_inputSystem->Initialize(window->GetGLFWHandle());
        }

        m_windows.push_back(window);
        LOG_INFO("Created window: " << info.Title << " (" << info.Width << "x" << info.Height << ")");

        return window;
    }

    /**
     * @brief Destroys a window previously created by this context and removes it from
     *        the internal window list.
     * @param window Pointer to the IWindow to destroy; no-op if null or unrecognized.
     */
    void GLFWPlatformContext::DestroyWindow(IWindow* window) {
        if (!window) return;

        auto it = std::find(m_windows.begin(), m_windows.end(), window);
        if (it != m_windows.end()) {
            if (auto* glfwWindow = dynamic_cast<GLFWWindow*>(window)) {
                glfwWindow->Destroy();
            }
            delete window;
            m_windows.erase(it);
            LOG_INFO("Destroyed window");
        }
    }

    /**
     * @brief Returns the first window created by this context.
     * @return Pointer to the primary IWindow, or nullptr if no windows exist.
     */
    IWindow* GLFWPlatformContext::GetMainWindow() const {
        return m_windows.empty() ? nullptr : m_windows[0];
    }

    /**
     * @brief Returns a reference to the list of all windows owned by this context.
     * @return Const reference to the internal window pointer vector.
     */
    const std::vector<IWindow*>& GLFWPlatformContext::GetAllWindows() const {
        return m_windows;
    }

    /**
     * @brief Returns the render device associated with this platform context.
     * @return Pointer to the IRenderDevice, or nullptr if not yet initialized.
     */
    IRenderDevice* GLFWPlatformContext::GetRenderDevice() const {
        return m_renderDevice.get();
    }

    /**
     * @brief Returns the input system associated with this platform context.
     * @return Pointer to the IInputSystem, or nullptr if not yet initialized.
     */
    IInputSystem* GLFWPlatformContext::GetInputSystem() const {
        return m_inputSystem.get();
    }

    /**
     * @brief Returns a human-readable identifier for this platform backend.
     * @return String containing "GLFW".
     */
    std::string GLFWPlatformContext::GetPlatformName() const {
        return "GLFW";
    }

    /**
     * @brief Returns the version string reported by the linked GLFW library.
     * @return GLFW version string (e.g., "3.4.0 Win32 WGL EGL OSMesa").
     */
    std::string GLFWPlatformContext::GetPlatformVersion() const {
        return glfwGetVersionString();
    }

}
