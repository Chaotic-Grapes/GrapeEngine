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
#include "services/Time.h"
#include "services/WindowManager.h"
#include <thread>
#include <filesystem>
#include "ecs/systems/UIEventSystem.h"
#include "services/UIEvents.h"

// Undefine potential Windows macros that conflict with enum names
#ifdef ERROR
#undef ERROR
#endif

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    void Application::Run(Game& game, const bool consoleFlag) {
        Time::_initialize();
        bool wasPlaying = false;

        // Set global pointer to this application instance
        CORE = this;

        // Initialize crash dumping system
        CrashDumping::Initialize();
        CrashDumping::SetProgramName("GrapeEngine");
        CrashDumping::SetDumpCreateState(true);


        // Initialize the message system here
        // Subscribe to Events
        //auto& bus = EventBus::Get();

        // Subscribe to action events
        /*bus.subscribe<ActionPressed>([this](const ActionPressed& e) {
            auto& busRef = EventBus::Get();
            if (e.action == "ActiveCameraSwitch") {
            std::cout << 'event action pressed';

            }
        }*/


        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
            
        // Initialize services
		_initializeServices();

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);

        m_lastFrameTime = glfwGetTime();
        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            const double rawDelta = frameStart - m_lastFrameTime;
            m_lastFrameTime = frameStart;

            // IMPORTANT NOTE: Anything to do with the UIEventQueue in this while loop
            // is only temporary! The proper handling of UI events should be done
            // by a centralized event system that is registered as a PROPER system.
            // TODO: Make centralized event system for UI events and others, and as
            // as a proper system
            ECS::UIEventQueue::Clear();
            
            Time::_update(rawDelta, frameStart);
            Profiler::UpdateTime();

            // if (5 second passed ){
               // publish the event
                //bus.publish(ActionPressed{ actionName });
            //}

            // --- Input & Game Update ---
            Input::_processInput();
			
			// Add fullscreen toggle (F11 key)
			if (Input::IsKeyPressed(GLFW_KEY_F11)) {
				Window* mainWindow = WindowManager::GetMainWindow();
				if (mainWindow) {
					if (mainWindow->HasMode(WindowMode::Fullscreen)) {
						mainWindow->SetMode(WindowMode::Windowed);
					} else {
						mainWindow->SetMode(WindowMode::Fullscreen);
					}
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
                const float dt = static_cast<float>(Time::DeltaTime());
                m_systemManager.Update(world, dt);
            }
            
            // Game-level update hook
            game.OnUpdate(m_sceneManager);

            // --- Rendering ---
            for (const auto* win : WindowManager::GetWindows()) {
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
                win->SwapBuffers();
            }

            // --- FPS Controller ---
            if (Time::FpsCap() > 0) {
                const double frameDuration = glfwGetTime() - frameStart;
                const double targetFrameTime = 1.0 / Time::FpsCap();
                if (frameDuration < targetFrameTime) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(targetFrameTime - frameDuration)
                    );
                }
            }
        }

        game.OnShutdown(m_sceneManager);

        // Clean up services
        if (m_scriptManager) {
            m_scriptManager->ShutdownCLR();
            delete m_scriptManager;
            m_scriptManager = nullptr;
        }
        delete m_audio;

        WindowManager::DestroyAll();
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

        // Register all ECS systems globally
        _registerSystems();
        
        // Initialize all systems
        ECS::World emptyWorld;
        m_systemManager.CreateAll(emptyWorld);
        LOG_INFO("SystemManager: Initialized " << m_systemManager.GetSystemCount() << " systems");
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
            m_accumulator += Time::UnscaledDeltaTime();

            const float maxAccumulator = Time::UnscaledFixedDeltaTime() * 5.0f;
            if (m_accumulator > maxAccumulator)
                m_accumulator = maxAccumulator;

            while (m_accumulator >= Time::UnscaledFixedDeltaTime()) {
                m_accumulator -= Time::UnscaledFixedDeltaTime();
            }
        }
        // Editor-only: Single step when paused (callback already consumed the flag)
        else if (stepRequested) {
            // Step request was already handled/cleared by callback
        }
    }

}
