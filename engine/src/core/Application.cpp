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
#include "core/Profiler.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/AnimationSystem.h"
#include "scripting/ScriptManager.h"
#include "scripting/ComponentTypeRegistry.h"
#include "ecs/systems/LifetimeSystem.h"
#include "ecs/systems/AudioSystem.h"
#include "scene/Scene.h"
#include "services/Input.h"
#include "services/TimeSystem.h"
#include <thread>
#include <filesystem>
#include "ecs/systems/UIEventSystem.h"
#include "services/UIEvents.h"
#include "platform/glfw/GLFWPlatformContext.h"

// Undefine potential Windows macros that conflict with enum names
#ifdef ERROR
#undef ERROR
#endif

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;

    void Application::Initialize(EngineMode mode, bool enableConsole) {
        if (m_initialized) {
            LOG_WARNING("Application already initialized");
            return;
        }

        m_mode = mode;
        m_shouldStop = false;

        // Set global pointer to this application instance
        CORE = this;

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

        // Initialize last-frame timestamp using the platform steady clock
        using Clock = TimeSystem::Clock;
        using Duration = TimeSystem::Duration;
        m_lastFrameTime = std::chrono::duration_cast<Duration>(Clock::now().time_since_epoch()).count();
        m_initialized = true;

        LOG_INFO("Engine initialized in " << (mode == EngineMode::Editor ? "Editor" : "Game") << " mode");
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

        // Clear UI event queue
        ECS::UIEventQueue::Clear();
        
        // Update time using platform timestamp and computed delta
        TimeSystem::Instance().Advance(rawDelta, frameStart);
        Profiler::UpdateTime();

        // --- Input Processing ---
        Input::_processInput();
        
        // Add fullscreen toggle (F11 key)
        if (Input::IsKeyPressed(GLFW_KEY_F11)) {
            auto* mainWindow = m_platformContext->GetMainWindow();
            if (mainWindow) {
                // Toggle fullscreen via platform abstraction
                // Note: IWindow doesn't expose mode switching yet - keeping legacy for now
                LOG_WARNING("Fullscreen toggle not yet implemented in platform abstraction");
            }
        }

        // --- Update Services ---
        m_audio->Update();

        // --- Scene Update ---
        auto* currentScene = m_sceneManager.GetActive();
        
        if (currentScene) {
            const bool shouldRun = _shouldRunGameLogic();
            const bool stepRequested = (m_stepRequestCallback && m_stepRequestCallback());
            auto& world = currentScene->GetWorld();

            // Handle game state transitions (start/stop)
            static bool wasPlaying = false;
            if (shouldRun && !wasPlaying) {
                LOG_INFO("Game started playing");
                _onGameStart(currentScene);
            }
            else if (!shouldRun && wasPlaying) {
                LOG_INFO("Game stopped/paused");
                _onGameStop(currentScene);
            }
            wasPlaying = shouldRun;

            uint32_t pickedEntityID = 0;  // TODO: Get from renderer

            double mouseX, mouseY;
            Input::GetMousePosition(mouseX, mouseY);
            Vector2D mousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));

            ECS::UIEventSystem::Update(&world, pickedEntityID, mousePos);

            // Update physics
            _updatePhysics(world, shouldRun, stepRequested);
            
            // Update all systems - they control their own run mode behavior
            const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
            m_systemManager.Update(world, dt);
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

        // Clean up services
        if (m_scriptManager) {
            m_scriptManager->ShutdownCLR();
            delete m_scriptManager;
            m_scriptManager = nullptr;
        }
        delete m_audio;
        m_audio = nullptr;

        // Shutdown platform context (handles window cleanup)
        if (m_platformContext) {
            m_platformContext->Shutdown();
            delete m_platformContext;
            m_platformContext = nullptr;
        }

        delete m_audio;
        m_audio = nullptr;

        m_initialized = false;
        CORE = nullptr;

        LOG_INFO("Engine shutdown complete");
    }

    void Application::Close() {
        m_shouldStop = true;
    }

    bool Application::LoadProjectSettings(const std::string& projectRoot) {
        std::string settingsPath = projectRoot + "/ProjectSettings.json";
        m_hasProjectSettings = Serialization::ConfigurationSerializer::LoadProjectSettings(settingsPath, m_projectSettings);
        if (m_hasProjectSettings) {
            LOG_INFO("Loaded project settings from: " << settingsPath);
        }
        else {
            LOG_WARNING("Failed to load project settings from: " << settingsPath);
        }
        return m_hasProjectSettings;
    }

    bool Application::SaveProjectSettings(const std::string& projectRoot) {
        try {
            std::filesystem::path settingsPath = std::filesystem::path(projectRoot) / "ProjectSettings.json";

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
        m_systemManager.RegisterSystem<ECS::LifetimeSystem>();
        m_systemManager.RegisterSystem<ECS::AnimationSystem>();
        m_systemManager.RegisterSystem<ECS::AudioSystem>(*m_audio);
        
        // Physics Phase Systems
        m_systemManager.RegisterSystem<ECS::PhysicsSystem>();
        
        // Render Phase Systems
        m_systemManager.RegisterSystem<ECS::RendererSystem>();
        
        // UIEventSystem is initialized separately
        ECS::UIEventSystem::Initialize();
        
        LOG_INFO("SystemManager: Registered " << m_systemManager.GetSystemCount() << " systems");
    }

    void Application::_onGameStart(Scenes::Scene* scene) {
        if (!scene) return;

        // Notify AudioSystem of scene start
        auto* audioSys = m_systemManager.GetSystem<ECS::AudioSystem>();
        if (audioSys) {
            audioSys->OnSceneStart();
            LOG_DEBUG("AudioSystem: Notified of scene start");
        }

    }

    void Application::_onGameStop(Scenes::Scene* scene) {
        if (!scene) return;

        m_accumulator = 0.0f; // Reset accumulator on stop

        auto* audioSystem = m_systemManager.GetSystem<ECS::AudioSystem>();
        if (audioSystem) {
            audioSystem->OnSceneStop();
            LOG_DEBUG("AudioSystem: Notified of scene stop");
        }
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

    // -------------------------------------------------------------------------
    // Game Loop Helper Methods
    // -------------------------------------------------------------------------

    bool Application::_shouldRunGameLogic() const {
        // Use callback if set (for editor control), otherwise always run
        if (m_gameLogicCallback) {
            return m_gameLogicCallback();
        }
        return true;
    }

    void Application::_updatePhysics(ECS::World& world, bool shouldRun, bool stepRequested) {
        // Handle fixed timestep accumulation for physics
        // Note: Physics system execution is handled by UpdateSystemsForMode() with SystemRunMode::PlayOnly
        
        if (!shouldRun && !stepRequested) {
            return; // Don't accumulate time when not playing
        }

        // Run physics at fixed timestep when playing
        if (shouldRun) {
            m_accumulator += static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());

            const float maxAccumulator = static_cast<float>(TimeSystem::Instance().GetFixedTimeStep()) * 5.0f;
            if (m_accumulator > maxAccumulator)
                m_accumulator = maxAccumulator;

            while (m_accumulator >= static_cast<float>(TimeSystem::Instance().GetFixedTimeStep())) {
                m_accumulator -= static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
            }
        }
        // Editor-only: Single step when paused (callback already consumed the flag)
        else if (stepRequested) {
            // Step request was already handled/cleared by callback
        }
    }

}
