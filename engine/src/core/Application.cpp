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
        Platform::WindowMode ResolveWindowMode(const std::string& mode) {
            if (mode == "Fullscreen") {
                return Platform::WindowMode::Fullscreen;
            }
            if (mode == "Borderless") {
                return Platform::WindowMode::Borderless;
            }
            return Platform::WindowMode::Windowed;
        }

        // Applies software frame cap from project settings to TimeSystem.
        // When VSync is enabled, disable software FPS capping.
        void ApplyFrameCapFromProjectSettings(const ProjectSettings& settings, bool vsyncEnabled) {
            if (vsyncEnabled) {
                TimeSystem::Instance().SetMaximumFPS(0.0);
                return;
            }

            const int maxFps = settings.MaxFPS > 0 ? settings.MaxFPS : 0;
            TimeSystem::Instance().SetMaximumFPS(static_cast<double>(maxFps));
        }
    }

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
        
        // Add fullscreen toggle (F11 key)
        if (Input::IsKeyPressed(GLFW_KEY_F11)) {
            auto* mainWindow = m_platformContext->GetMainWindow();
            if (mainWindow) {
                // Toggle fullscreen via platform abstraction
                // Note: IWindow doesn't expose mode switching yet - keeping legacy for now
                LOG_WARNING("Fullscreen toggle not yet implemented in platform abstraction");
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
                // Update physics
                _updatePhysics(world);
                
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

    void Application::Close() {
        m_shouldStop = true;
    }

    bool Application::LoadProjectSettings(const std::string& projectRoot) {
        if (!ProjectPaths::IsInitialized()) {
            LOG_ERROR("ProjectPaths not initialized; cannot load project settings");
            return false;
        }

        std::string settingsPath = ProjectPaths::GetSettingsPath();
        if (m_mode == EngineMode::Game && !projectRoot.empty()) {
            std::filesystem::path localPath = std::filesystem::path(projectRoot) / "ProjectSettings.json";
            if (std::filesystem::exists(localPath)) {
                settingsPath = localPath.string();
            }
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

    bool Application::SaveProjectSettings(const std::string& projectRoot) {
        try {
            if (!ProjectPaths::IsInitialized()) {
                LOG_ERROR("ProjectPaths not initialized; cannot save project settings");
                return false;
            }

            std::filesystem::path settingsPath = ProjectPaths::GetSettingsPath();
            if (m_mode == EngineMode::Game && !projectRoot.empty()) {
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

    void Application::UpdateSystemsByMode(uint32_t modes, ECS::World& world) {
        // Public API for editor to directly control which modes execute
        // This allows editor to implement play/pause/step/edit state transitions
        m_systemManager.SetActiveRunModeMask(modes);

        if (modes & (1 << static_cast<int>(ECS::SystemRunMode::Always))) {
            m_systemManager.UpdateSystemsForMode(ECS::SystemRunMode::Always, world);
        }
        if (modes & (1 << static_cast<int>(ECS::SystemRunMode::PlayOnly))) {
            m_systemManager.UpdateSystemsForMode(ECS::SystemRunMode::PlayOnly, world);
        }
        if (modes & (1 << static_cast<int>(ECS::SystemRunMode::EditOnly))) {
            m_systemManager.UpdateSystemsForMode(ECS::SystemRunMode::EditOnly, world);
        }
    }

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

    void Application::_enableConsole() {
#ifdef _WIN32
#include <windows.h>
        AllocConsole();

        FILE* dummy;
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stderr));
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stdout));
#endif
    }

    void Application::_disableConsole() {
#ifdef _WIN32
        if (const HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
#endif
    }

    void Application::_updatePhysics(ECS::World& /*world*/) {
        // Handle fixed timestep accumulation for physics
        // Only called in Game mode - always accumulates
        
        m_accumulator += static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());

        const float maxAccumulator = static_cast<float>(TimeSystem::Instance().GetFixedTimeStep()) * 5.0f;
        if (m_accumulator > maxAccumulator)
            m_accumulator = maxAccumulator;

        while (m_accumulator >= static_cast<float>(TimeSystem::Instance().GetFixedTimeStep())) {
            m_accumulator -= static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
        }
    }

    // ==================== Device Management ====================

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

    MonitorInfo Application::GetCurrentMonitorInfo() const {
        auto monitors = DeviceManager::EnumerateMonitors();
        if (!monitors.empty()) {
            return monitors[0];  // Return primary monitor
        }
        return MonitorInfo{};
    }

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
