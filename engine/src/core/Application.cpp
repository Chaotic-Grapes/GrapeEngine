/* Start Header *****************************************************************/
/*!
\file   Application.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Implements the Application class which serves as the core of the engine,
managing the main loop, services, and scene management.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/Application.h"
#include "core/CrashDumping.h"
#include "core/messaging/MessageSystem.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/ParticleSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/BoidSystem.h"
#include "ecs/systems/GUILayoutSystem.h"
#include "ecs/systems/GUIInputSystem.h"
#include "ecs/systems/GUIRenderSystem.h"
#include "ecs/systems/AnimationSystem.h"
#include "ecs/systems/AnimationPreviewSystem.h"
#include "ecs/events/EventDispatcher.h"
#include "scripting/ScriptManager.h"
#include "scripting/ComponentTypeRegistry.h"
#include "ecs/systems/TransformSystem.h"
#include "ecs/systems/AudioSystem.h"
#include "scene/Scene.h"
#include "services/Input.h"
#include "services/MemoryManager.h"
#include "services/ResourceManager.h"
#include "services/TimeSystem.h"
#include <thread>
#include <filesystem>
#include "core/ProjectPaths.h"
#include "platform/glfw/GLFWPlatformContext.h"
#include "platform/glfw/GLFWWindow.h"

#ifdef GRAPE_HAS_CUDA
#include "cuda/CudaTest.cuh"
#endif

// Undefine potential Windows macros that conflict with enum names
#ifdef ERROR
#undef ERROR
#endif

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;

    namespace {

        /** @brief Converts a window mode string from project settings into a platform enum value.
         *  @param mode String representation of the window mode ("Fullscreen", "Borderless", or any other value for Windowed).
         *  @return The corresponding Platform::WindowMode enum value.
         */
        Platform::WindowMode ResolveWindowMode(const std::string& mode) {
            if (mode == "Fullscreen") {
                return Platform::WindowMode::Fullscreen;
            }
            if (mode == "Borderless") {
                return Platform::WindowMode::Borderless;
            }
            return Platform::WindowMode::Windowed;
        }

        /** @brief Applies the software frame cap from project settings to the TimeSystem.
         *
         *  When VSync is enabled the software FPS cap is disabled (set to 0). Otherwise the
         *  MaxFPS value from the project settings is forwarded to TimeSystem.
         *
         *  @param settings      Project settings that carry the MaxFPS value.
         *  @param vsyncEnabled  True when the platform window has VSync active.
         */
        void ApplyFrameCapFromProjectSettings(const ProjectSettings& settings, bool vsyncEnabled) {
            if (vsyncEnabled) {
                TimeSystem::Instance().SetMaximumFPS(0.0);
                return;
            }

            const int maxFps = settings.MaxFPS > 0 ? settings.MaxFPS : 0;
            TimeSystem::Instance().SetMaximumFPS(static_cast<double>(maxFps));
        }
    }

    /** @brief Initializes the engine and all subsystems.
     *
     *  Sets up crash dumping, the time system, the platform context (windowing, input, rendering),
     *  all engine services, and broadcasts the ApplicationStart event. Safe to call only once;
     *  subsequent calls are ignored.
     *
     *  @param mode           Operating mode: EngineMode::Editor or EngineMode::Game.
     *  @param enableConsole  When true a console window is allocated; otherwise it is hidden.
     */
    void Application::Initialize(EngineMode mode, bool enableConsole) {
        if (m_initialized) {
            LOG_WARNING("Application already initialized");
            return;
        }

        #ifdef GRAPE_HAS_CUDA
                m_cudaAvailable = CudaTestRun();
                if (!m_cudaAvailable) {
                    LOG_WARNING("CUDA runtime test failed. BoidSystem will be disabled on this machine.");
                } else {
                    LOG_INFO("CUDA runtime test passed. BoidSystem enabled.");
                }
        #endif

        m_mode = mode;
        m_shouldStop = false;
        m_notifiedActiveSceneIndex = static_cast<size_t>(-1);

        // Set global pointer to this application instance
        CORE = this;

        // Force memory manager pool creation at load time
        (void)MemoryManager::GetInstance();

        // Initialize crash dumping system
        CrashDumping::Initialize();
        CrashDumping::SetProgramName("GrapeEngine");
        CrashDumping::SetDumpCreateState(true);

        // Initialize time system
        TimeSystem::Instance().Start();

        // Enable/disable console
        if (enableConsole)
            _enableConsole();
        else
            _disableConsole();

        // Initialize platform context (GLFW, windowing, input, rendering)
        m_platformContext = new Platform::GLFWPlatformContext();
        if (!m_platformContext->Initialize()) {
            LOG_ERROR("Failed to initialize platform context");
            delete m_platformContext;
            m_platformContext = nullptr;
            return;
        }
        LOG_INFO("Platform context initialized successfully");

        // Initialize services
        _initializeServices();

        // Start audio device change detection
        DeviceManager::StartAudioDeviceChangeDetection();

        // Initialize last-frame timestamp using the platform steady clock
        using Clock = TimeSystem::Clock;
        using Duration = TimeSystem::Duration;
        m_lastFrameTime = std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
        m_initialized = true;

        LOG_INFO("Engine initialized in " << (mode == EngineMode::Editor ? "Editor" : "Game") << " mode");

        // Broadcast application start event
        Messaging::MessageSystem::Notify(Messaging::ApplicationStart{});
    }

    /** @brief Executes one full engine frame.
     *
     *  Advances the time system, processes input, updates the scene manager and all active ECS
     *  systems, handles scene lifecycle transitions in game mode, updates the audio service, and
     *  checks whether any platform window has requested a close.
     */
    void Application::Update() {
        if (!m_initialized) {
            LOG_ERROR("Application::Update called before Initialize");
            return;
        }

        // Compute platform steady-clock timestamp for this frame and delta
        using Clock = TimeSystem::Clock;
        using Duration = TimeSystem::Duration;
        const double frameStart = std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
        const double rawDelta = frameStart - m_lastFrameTime;
        m_lastFrameTime = frameStart;

        // Apply runtime frame cap from project performance settings.
        // Editor mode stays uncapped by default.
        if (m_mode == EngineMode::Game && m_hasProjectSettings) {
            bool vsyncEnabled = m_projectSettings.WindowSettings.VSync;
            if (m_platformContext) {
                if (auto* window = m_platformContext->GetMainWindow()) {
                    vsyncEnabled = window->IsVSync();
                }
            }
            ApplyFrameCapFromProjectSettings(m_projectSettings, vsyncEnabled);
        } else {
            TimeSystem::Instance().SetMaximumFPS(0.0);
        }

        // Update time using platform timestamp and computed delta
        TimeSystem::Instance().Advance(rawDelta, frameStart);

        // Apply FPS cap if set by sleeping the remainder of the frame
        double maximumFPS = TimeSystem::Instance().GetMaximumFPS();
        if (maximumFPS > 0.0) {
            double minFrameTime = 1.0 / maximumFPS;
            double elapsedTime = std::chrono::duration_cast<TimeSystem::Duration>(
                TimeSystem::Clock::now().time_since_epoch()
            ).count() - frameStart;

            if (elapsedTime < minFrameTime) {
                double sleepTime = minFrameTime - elapsedTime;
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
            }
        }

        // --- Input Processing ---
        Input::_processInput();
        
        // --- Device Change Detection ---
        if (DeviceManager::HasAudioDevicesChanged()) {
            LOG_CRITICAL("Audio devices changed - audio may not be playing correctly");
            // CRITICAL TODO: reinitialize audio service
        }
        
        // Runtime fullscreen toggle hotkeys:
        // - Alt+Enter (requested game-style shortcut)
        // - F11 (kept for convenience/debug parity)
        const bool altHeld = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
        const bool enterPressed = Input::IsKeyPressed(KEY_ENTER) || Input::IsKeyPressed(GLFW_KEY_KP_ENTER);
        const bool altEnterPressed = altHeld && enterPressed;
        const bool f11Pressed = Input::IsKeyPressed(KEY_F11);
        if (altEnterPressed || f11Pressed) {
            if (auto* mainWindow = m_platformContext->GetMainWindow()) {
                const bool currentlyFullscreen = mainWindow->HasMode(Platform::WindowMode::Fullscreen);
                const bool targetFullscreen = !currentlyFullscreen;
                if (SetFullscreenMode(targetFullscreen)) {
                    m_projectSettings.WindowSettings.Fullscreen = targetFullscreen;
                    m_projectSettings.WindowSettings.Mode = targetFullscreen ? "Fullscreen" : "Windowed";
                }
            }
        }

        // --- Scene Update ---
        m_sceneManager.Update();

        // In game mode, broadcast scene lifecycle callbacks when active scene changes.
        // Editor mode handles scene lifecycle from the editor playback state machine.
        const size_t currentActiveSceneIndex = m_sceneManager.GetActiveIndex();
        if (m_mode == EngineMode::Game && currentActiveSceneIndex != m_notifiedActiveSceneIndex) {
            if (m_notifiedActiveSceneIndex != static_cast<size_t>(-1)) {
                const Scenes::Scene* previousScene = m_sceneManager.GetScene(m_notifiedActiveSceneIndex);
                if (previousScene) {
                    m_systemManager.OnSceneStop(const_cast<Scenes::Scene*>(previousScene)->GetWorld());
                }
            }

            if (auto* nextScene = m_sceneManager.GetActive()) {
                m_systemManager.OnSceneStart(nextScene->GetWorld());
            }

            m_notifiedActiveSceneIndex = currentActiveSceneIndex;
        }

        // --- Update Services ---
        m_audio->Update();
        auto* currentScene = m_sceneManager.GetActive();
        
        if (currentScene) {
            auto& world = currentScene->GetWorld();

            uint32_t pickedEntityID = 0;  // TODO: Get from renderer
			(void)pickedEntityID;

            double mouseX, mouseY;
            Input::GetMousePosition(mouseX, mouseY);
            Vector2D mousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));

            // Game mode: always run systems
            // Editor mode: editor controls system execution via UpdateSystemsByMode()
            if (m_mode == EngineMode::Game) {
                // Update systems - always run for game mode
                m_systemManager.UpdateWithDependencies(world);
                ECS::Events::ClearFrameEventComponents(world);
            }
        }

        // Check for window close
        for (const auto* win : m_platformContext->GetAllWindows()) {
            if (win->ShouldClose()) {
                m_shouldStop = true;
                break;
            }
        }
    }

    /** @brief Resynchronize internal frame timestamp to the current steady clock.
     *  @return void
     *  @note Use this after long runtime interruptions so the next Update() sees a near-zero delta.
     */
    void Application::ResyncFrameTime() {
        using Clock = TimeSystem::Clock;
        using Duration = TimeSystem::Duration;
        m_lastFrameTime = std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
    }

    /** @brief Tears down all engine subsystems and releases all resources.
     *
     *  Broadcasts the ApplicationExit event, destroys ECS systems, clears the resource cache,
     *  shuts down scripting and audio services, and shuts down the platform context. Idempotent
     *  when called on an already-shutdown application.
     */
    void Application::Shutdown() {
        if (!m_initialized) {
            return;
        }

        LOG_INFO("Shutting down engine...");

        // Broadcast application exit event
        Messaging::MessageSystem::Notify(Messaging::ApplicationExit{});

        // Stop device change detection
        DeviceManager::StopAudioDeviceChangeDetection();

        // Destroy ECS systems before tearing down rendering/audio backends.
        ECS::World emptyWorld;
        if (auto* activeScene = m_sceneManager.GetActive()) {
            m_systemManager.DestroyAll(activeScene->GetWorld());
        } else {
            m_systemManager.DestroyAll(emptyWorld);
        }

        // Release cached graphics/audio resources while the backend is alive.
        RM.ClearCache();

        // Clean up services
        if (m_scriptManager) {
            m_scriptManager->ShutdownCLR();
            delete m_scriptManager;
            m_scriptManager = nullptr;
        }
        if (m_audio) {
            m_audio->Terminate();
            delete m_audio;
            m_audio = nullptr;
        }

        // Shutdown platform context (handles window cleanup)
        if (m_platformContext) {
            m_platformContext->Shutdown();
            delete m_platformContext;
            m_platformContext = nullptr;
        }

        m_initialized = false;
        m_notifiedActiveSceneIndex = static_cast<size_t>(-1);
        CORE = nullptr;

        LOG_INFO("Engine shutdown complete");
    }

    /** @brief Requests that the engine main loop stop at the end of the current frame.
     */
    void Application::Close() {
        m_shouldStop = true;
    }

    /** @brief Loads project settings from disk and applies them to the active window.
     *
     *  Reads ProjectSettings.json from the given project root (or the path resolved by ProjectPaths
     *  when the argument is empty). In Game mode the loaded settings are immediately forwarded to
     *  the main platform window (title, VSync, mode, resolution).
     *
     *  @param projectRoot Optional override directory that contains ProjectSettings.json. Pass an
     *                     empty string to use the path returned by ProjectPaths::GetSettingsPath().
     *  @return True if the settings file was read and applied successfully, false otherwise.
     */
    bool Application::LoadProjectSettings(const std::string& projectRoot) {
        if (!ProjectPaths::IsInitialized()) {
            LOG_ERROR("ProjectPaths not initialized; cannot load project settings");
            return false;
        }

        std::string settingsPath = ProjectPaths::GetSettingsPath();
        if (!projectRoot.empty()) {
            settingsPath = (std::filesystem::path(projectRoot) / "ProjectSettings.json").string();
        }
        m_hasProjectSettings = Serialization::ConfigurationSerializer::LoadProjectSettings(settingsPath, m_projectSettings);
        if (m_hasProjectSettings) {
            LOG_INFO("Loaded project settings from: " << settingsPath);

            // Apply loaded settings to main window if in game mode
            if (m_mode == EngineMode::Game && m_platformContext) {
                auto* window = m_platformContext->GetMainWindow();

                // Apply project settings to window (title, VSync, mode, resolution)
                if (window) {
                    window->SetTitle(m_projectSettings.Title);
                    window->SetVSync(m_projectSettings.WindowSettings.VSync);
                    window->SetMode(ResolveWindowMode(m_projectSettings.WindowSettings.Mode));

                    // If starting in windowed mode, apply resolution settings. 
                    // Fullscreen and borderless modes will use the monitor's native resolution.
                    if (m_projectSettings.WindowSettings.Mode == "Windowed") {
                        window->Resize(m_projectSettings.WindowSettings.Width, m_projectSettings.WindowSettings.Height);
                    }
                }
            }
        }
        else {
            LOG_WARNING("Failed to load project settings from: " << settingsPath);
        }
        return m_hasProjectSettings;
    }

    /** @brief Serializes the current project settings to disk.
     *
     *  Writes the in-memory ProjectSettings to ProjectSettings.json inside the given project root
     *  directory (or the ProjectPaths default when the argument is empty). Parent directories are
     *  created automatically if they do not yet exist.
     *
     *  @param projectRoot Optional override directory in which to write ProjectSettings.json. Pass
     *                     an empty string to use the path returned by ProjectPaths::GetSettingsPath().
     *  @return True if the file was written successfully, false on any error.
     */
    bool Application::SaveProjectSettings(const std::string& projectRoot) {
        try {
            if (!ProjectPaths::IsInitialized()) {
                LOG_ERROR("ProjectPaths not initialized; cannot save project settings");
                return false;
            }

            std::filesystem::path settingsPath = ProjectPaths::GetSettingsPath();
            if (!projectRoot.empty()) {
                settingsPath = std::filesystem::path(projectRoot) / "ProjectSettings.json";
            }

            // Ensure parent directories exist
            std::filesystem::path parent = settingsPath.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                std::filesystem::create_directories(parent);
            }

            const std::string settingsPathStr = settingsPath.string();
            if (Serialization::ConfigurationSerializer::SaveProjectSettings(settingsPathStr, m_projectSettings)) {
                LOG_INFO("Saved project settings to: " << settingsPathStr);
                return true;
            }
            else {
                LOG_ERROR("Failed to save project settings to: " << settingsPathStr);
                return false;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception saving project settings: " << e.what());
            return false;
        }
    }

    /** @brief Updates ECS systems for the specified run-mode mask within the given world.
     *
     *  Allows the editor to drive system execution explicitly (play / pause / step / edit
     *  state transitions) rather than relying on the automatic game-mode update path.
     *
     *  @param modes Bitmask of run modes (SystemRunMode flags) that should execute this frame.
     *  @param world The ECS world whose systems will be ticked.
     */
    void Application::UpdateSystemsByMode(uint32_t modes, ECS::World& world) {
        // Public API for editor to directly control which modes execute
        // This allows editor to implement play/pause/step/edit state transitions
        m_systemManager.SetActiveRunModeMask(modes);

        // Execute all active modes in one traversal to reduce per-frame overhead.
        m_systemManager.UpdateSystemsByMask(modes, world);
    }

    /** @brief Creates and initializes all engine services (audio, scripting, ECS systems).
     *
     *  Instantiates the AudioService, registers C++ component types for C# interop, initializes
     *  the ScriptManager and loads the scripting assembly, then calls _registerSystems() to
     *  register all built-in ECS systems with the system manager.
     */
    void Application::_initializeServices() {
        m_audio = new Services::AudioService();
        m_audio->Initialize();

        // Register all C++ component types with hash mapping for C# interop
        // This MUST be done before initializing ScriptManager
        ECS::RegisterAllComponentTypes();
        LOG_INFO("Component types registered for C# interop");

        // Initialize C# scripting via ScriptManager
        m_scriptManager = new ECS::ScriptManager();
        if (!m_scriptManager->InitializeCLR("GrapeEngine.Scripting.runtimeconfig.json")) {
            LOG_WARNING("Failed to initialize ScriptManager/CLR");
        }
        else {
            LOG_INFO("ScriptManager initialized successfully");
            
            // Load the script API assembly
            if (!m_scriptManager->LoadAssembly("GrapeEngine.Scripting.dll")) {
                LOG_WARNING("Failed to load Scripting assembly");
            }
            else {
                LOG_INFO("Scripting assembly loaded");
                
                // Discover and register all C# systems
                int systemCount = m_scriptManager->RegisterScriptedSystems(m_systemManager);
                LOG_INFO("ScriptManager: Registered " << systemCount << " C# systems");
            }
        }

        // Register all ECS systems globally (do NOT initialize them yet)
        // Systems that depend on an OpenGL context (RendererSystem) expect a
        // main window to exist before OnCreate() is called. Creating systems
        // here (before Game::OnStart) may run them before the window is
        // created which leads to null-pointer access. Defer CreateAll until
        // after Game::OnStart so the game/editor can create windows first.
        _registerSystems();
    }

    /** @brief Registers all built-in ECS systems with the system manager and builds dependency graphs.
     *
     *  Systems are registered in execution-order groups (Update, Physics, Render). Dependency graphs
     *  are built after registration so the system manager can schedule parallel execution correctly.
     *  BoidSystem registration is conditional on CUDA availability.
     */
    void Application::_registerSystems() {
        // Register all ECS systems in order
        // Systems will execute based on their SystemGroup and executionOrder
        
        // Update Phase Systems
        m_systemManager.RegisterSystem<ECS::AnimationSystem>();
        m_systemManager.RegisterSystem<ECS::AnimationPreviewSystem>();
        auto* audioSystem = m_systemManager.RegisterSystem<ECS::AudioSystem>(*m_audio);
        m_sceneManager.SetAudioSystem(audioSystem);
        #ifdef GRAPE_HAS_CUDA
        if (m_cudaAvailable) {
            m_systemManager.RegisterSystem<ECS::BoidSystem>();
        } else {
            LOG_WARNING("SystemManager: Skipping BoidSystem registration (CUDA unavailable)");
        }
        #else
        LOG_INFO("SystemManager: Skipping BoidSystem registration (engine built without CUDA)");
        #endif

        // Physics Phase Systems
        // Ensure transform propagation updated before physics runs
        m_systemManager.RegisterSystem<ECS::TransformSystem>();
        m_systemManager.RegisterSystem<ECS::PhysicsSystem>();
        
        // Render Phase Systems
        m_systemManager.RegisterSystem<ECS::ParticleSystem>();
        m_systemManager.RegisterSystem<ECS::RendererSystem>();
        m_systemManager.RegisterSystem<ECS::GUILayoutSystem>();
        m_systemManager.RegisterSystem<ECS::GUIInputSystem>();
        m_systemManager.RegisterSystem<ECS::GUIRenderSystem>();

        // Build dependency graphs (analyzes component access)
        m_systemManager.BuildDependencyGraphs();
        
        LOG_INFO("SystemManager: Registered " << m_systemManager.GetSystemCount() << " systems");
    }

    /** @brief Allocates and shows a console window on Windows and redirects stdout/stderr to it.
     *
     *  No-op on non-Windows platforms.
     */
    void Application::_enableConsole() {
#ifdef _WIN32
#include <windows.h>
        AllocConsole();

        FILE* dummy;
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stderr));
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stdout));
#endif
    }

    /** @brief Hides the console window on Windows.
     *
     *  No-op on non-Windows platforms.
     */
    void Application::_disableConsole() {
#ifdef _WIN32
        if (const HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
#endif
    }

    /** @brief Placeholder for legacy fixed-step physics dispatch (currently a no-op).
     *
     *  Fixed-step physics is fully owned by PhysicsSystem::OnUpdate and does not require a
     *  separate dispatch call from the application layer.
     *
     *  @param world The ECS world (unused).
     */
    void Application::_updatePhysics(ECS::World& /*world*/) {
        // Fixed-step physics is owned by PhysicsSystem::OnUpdate.
    }

    // ==================== Device Management ====================

    /** @brief Changes the rendering resolution if it is supported by the primary monitor.
     *
     *  Validates the requested resolution against the list of modes reported by
     *  DeviceManager::EnumerateMonitors(), then applies the viewport change via the render device.
     *
     *  @param width       Desired horizontal resolution in pixels.
     *  @param height      Desired vertical resolution in pixels.
     *  @param refreshRate Desired refresh rate (currently unused, reserved for future use).
     *  @return True if the resolution was applied successfully, false otherwise.
     */
    bool Application::SetResolution(int width, int height, int /*refreshRate*/) {
        if (!m_platformContext) {
            LOG_ERROR("Platform context unavailable");
            return false;
        }

        // Validate resolution is supported
        auto monitors = DeviceManager::EnumerateMonitors();
        if (monitors.empty()) {
            LOG_ERROR("No monitors found");
            return false;
        }

        // Check primary monitor
        const auto& primaryMonitor = monitors[0];
        bool resolutionSupported = false;

        for (const auto& mode : primaryMonitor.SupportedModes) {
            if (mode.Width == width && mode.Height == height) {
                resolutionSupported = true;
                break;
            }
        }

        if (!resolutionSupported) {
            LOG_ERROR("Resolution " << width << "x" << height 
                << " not supported by current monitor");
            return false;
        }

        // Apply resolution via render device
        auto* renderDevice = m_platformContext->GetRenderDevice();
        if (renderDevice) {
            renderDevice->SetViewport(0, 0, width, height);
            LOG_INFO("Resolution changed to " << width << "x" << height);
            return true;
        }

        return false;
    }

    /** @brief Switches the main window into or out of fullscreen on the specified monitor.
     *
     *  Delegates to the platform window implementation. On GLFW the monitor index is passed
     *  directly; on other platforms the generic IWindow::SetFullscreen() call is used.
     *
     *  @param fullscreen   True to enter fullscreen, false to return to windowed mode.
     *  @param monitorIndex Zero-based index of the target monitor as enumerated by DeviceManager.
     *  @return True if the mode change was successful, false otherwise.
     */
    bool Application::SetFullscreenMode(bool fullscreen, int monitorIndex) {
        if (!m_platformContext) {
            LOG_ERROR("Platform context unavailable");
            return false;
        }

        auto monitors = DeviceManager::EnumerateMonitors();
        if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
            LOG_ERROR("Invalid monitor index: " << monitorIndex);
            return false;
        }

        // Get main window and delegate to platform implementation
        auto* mainWindow = m_platformContext->GetMainWindow();
        if (!mainWindow) {
            LOG_ERROR("Main window not available");
            return false;
        }

        // Use platform implementation to switch fullscreen mode
        if (auto* glfwWindow = dynamic_cast<Platform::GLFWWindow*>(mainWindow)) {
            if (glfwWindow->SetFullscreenOnMonitor(fullscreen ? monitorIndex : -1)) {
                LOG_INFO("Fullscreen mode switched to " << (fullscreen ? "on" : "off"));
                return true;
            }
        } else if (mainWindow->SetFullscreen(fullscreen)) {
            LOG_INFO("Fullscreen mode switched to " << (fullscreen ? "on" : "off"));
            return true;
        }

        LOG_ERROR("Failed to switch fullscreen mode");
        return false;
    }

    /** @brief Records the preferred audio output device for this session.
     *
     *  Validates that the device with the given ID exists and is connected, then stores the
     *  preference. Full FMOD device re-initialization is noted as a TODO; only the preference
     *  is persisted at this time.
     *
     *  @param deviceID Platform-specific device identifier string obtained from DeviceManager.
     *  @return True if the device was found, is connected, and the preference was stored;
     *          false on any validation or service error.
     */
    bool Application::SetAudioDevice(const std::string& deviceID) {
        // Validate device exists
        auto device = DeviceManager::GetAudioDeviceInfo(deviceID);
        if (device.DeviceID.empty()) {
            LOG_ERROR("Audio device not found: " << deviceID);
            return false;
        }

        if (!device.IsConnected) {
            LOG_ERROR("Audio device is not connected: " << device.DeviceName);
            return false;
        }

        // Request audio service to switch devices
        if (m_audio) {
            auto* audioDevice = m_audio->Device();
            if (audioDevice) {
                // For FMOD, device switching would require more complex setup
                // For now, we just track the preference
                m_currentAudioDeviceID = deviceID;
                LOG_INFO("Audio device preference set to: " << device.DeviceName <<
                         " (" << device.Channels << "ch @ " << device.SampleRate << "Hz)");
                LOG_WARNING("Note: Actual FMOD device switching requires re-initialization");
                return true;
            }
        }

        LOG_ERROR("Audio service or device unavailable");
        return false;
    }

    /** @brief Returns information about the primary monitor.
     *
     *  @return A MonitorInfo struct describing the primary monitor; a default-constructed
     *          MonitorInfo{} if no monitors are found.
     */
    MonitorInfo Application::GetCurrentMonitorInfo() const {
        auto monitors = DeviceManager::EnumerateMonitors();
        if (!monitors.empty()) {
            return monitors[0];  // Return primary monitor
        }
        return MonitorInfo{};
    }

    /** @brief Returns information about the currently active audio output device.
     *
     *  If a specific device ID has been set via SetAudioDevice() and that device still exists,
     *  its AudioDeviceInfo is returned. Otherwise the system default output device is returned.
     *
     *  @return AudioDeviceInfo for the active or default audio output device.
     */
    AudioDeviceInfo Application::GetCurrentAudioDevice() const {
        // If we have a specific device set, return its info
        if (!m_currentAudioDeviceID.empty()) {
            auto device = DeviceManager::GetAudioDeviceInfo(m_currentAudioDeviceID);
            if (!device.DeviceID.empty()) {
                return device;
            }
        }

        // Otherwise return the default device
        return DeviceManager::GetDefaultAudioOutputDevice();
    }
}
