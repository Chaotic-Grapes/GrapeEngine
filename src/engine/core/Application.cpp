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
#include "ecs/systems/ScriptSystem.h"
#include "audio/AudioSystem.h"
#include "audio/AudioSystemRegistry.h"
#include "scene/Scene.h"
#include "scene/SystemRegistry.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/WindowManager.h"
#include "services/OverlayService.h"
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

        // Load editor configuration (try working dir, then parent dir fallback)
        bool configLoaded = Serialization::ConfigurationSerializer::LoadConfig("config.json", m_editorSettings);
        if (!configLoaded) {
            // Fallback: common scenario when running from build directory
            if (Serialization::ConfigurationSerializer::LoadConfig("../config.json", m_editorSettings)) {
                LOG_INFO("Loaded configuration from parent directory: ../config.json");
            }
        }
        else {
            LOG_INFO("Loaded configuration file: " << std::filesystem::current_path().string() + "/config.json");
        }

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

        // Start with some systems disabled in editor mode
        if (IsInEditorMode()) {
            Scenes::SystemRegistry::Disable("Physics");
            Scenes::SystemRegistry::Disable("Lifetime");
            Scenes::SystemRegistry::Disable("Animation");
        }

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

            // --- Update Services ---
            m_audio->Update();

            // --- Scene Update ---
            auto* currentScene = m_sceneManager.GetActive();
            
            if (currentScene) {
                const bool shouldRun = _shouldRunGameLogic();
                const bool stepRequested = (m_overlay && m_overlay->IsStepRequested());
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

                // Update physics and scripts
                _updatePhysics(world, shouldRun, stepRequested);
                _updateScripts(world, shouldRun);
            }
            
            // Run all non-physics systems at variable timestep
            m_sceneManager.Update();
            
            // Game-level update hook
            game.OnUpdate(m_sceneManager);

            // --- Update Overlay Service ---
            // This here because it depends on current scene
			m_overlay->Update();
            m_overlay->Render();

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

        // Save configs before exiting
        if (IsInEditorMode()) {
            m_editorSettings.WindowSettings.Width = WindowManager::GetMainWindow()->GetWidth();
            m_editorSettings.WindowSettings.Height = WindowManager::GetMainWindow()->GetHeight();
            m_editorSettings.WindowSettings.Maximized = WindowManager::GetMainWindow()->IsMaximized();
            m_editorSettings.WindowSettings.VSync = WindowManager::GetMainWindow()->IsVSync();
            
            Serialization::ConfigurationSerializer::SaveConfig("config.json", m_editorSettings);
        }
        else {
            // TODO: Save standalone later
        }

        game.OnShutdown(m_sceneManager);

        // Clean up services
        if (m_scriptSystem) {
            m_scriptSystem->Shutdown();
            delete m_scriptSystem;
            m_scriptSystem = nullptr;
        }
        delete m_audio;
        delete m_overlay;

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

    #ifdef USE_IMGUI
        m_overlay = new Services::OverlayService(m_sceneManager);
        m_overlay->SetAudio(m_audio->Device());
        m_overlay->Initialize();
        // Set editor mode flag (overlay exists = editor mode)
        m_isInEditorMode = (m_overlay != nullptr);
    #else
        m_overlay = nullptr;
        m_isInEditorMode = false;
    #endif

        // Initialize scripting system
        m_scriptSystem = new ECS::ScriptSystem();
        if (!m_scriptSystem->Initialize("GrapeEngine.ScriptAPI.runtimeconfig.json")) {
            LOG_WARNING("Failed to initialize ScriptSystem");
        }
        else {
            LOG_INFO("ScriptSystem initialized successfully");
            // Load the script API assembly
            if (!m_scriptSystem->LoadAssembly("GrapeEngine.ScriptAPI.dll")) {
                LOG_WARNING("Failed to load ScriptAPI assembly");
            }
        }

        // Register all ECS systems
        _registerSystems();
    }

    void Application::_registerSystems() {
        // Physics
        Scenes::SystemRegistry::Register("Physics", [](ECS::World& w, const float dt) {
            ECS::PhysicsSystem::Update(w, dt);
        });

        // Lifetime
        Scenes::SystemRegistry::Register("Lifetime", [](ECS::World& w, const float dt) {
            ECS::LifetimeSystem::Update(w, dt);
        });

        // Animation
        Scenes::SystemRegistry::Register("Animation", [](ECS::World& w, const float dt) {
            ECS::AnimationSystem::Update(w, dt);
        });
        
        // Render
        Scenes::SystemRegistry::Register("Render", [this](ECS::World& w, const float dt) {
            // Only run renderer system in standalone mode
            // In editor mode, the Viewport handles rendering to avoid double updates
            if (!IsInEditorMode()) {
                static ECS::RendererSystem s_renderer;
                s_renderer.Initialize(w);
                s_renderer.BindWorld(w);
                s_renderer.Update(w, dt);
            }
        });

        // Register UI System soon

        // Store one AudioSystem instance per world to handle scene switching properly
        Scenes::SystemRegistry::Register("Audio", [this](ECS::World& w, const float dt) {
            auto* svc = m_audio;
            if (!svc) return;

            // Store AudioSystem per world to handle scene switching
            static std::unordered_map<ECS::World*, std::unique_ptr<AudioSystem>> s_audioSystems;

            auto it = s_audioSystems.find(&w);
            if (it == s_audioSystems.end()) {
                auto result = s_audioSystems.emplace(&w, std::make_unique<AudioSystem>(w, *svc));
                it = result.first;
                Audio::AUDIO_MAP[&w] = it->second.get();
                LOG_DEBUG("Audio system: Created AudioSystem for world at " << &w);
            }

            // Update the audio system
            if (it->second) {
                it->second->Update(dt);
            }
        });

        // Register scripting system
        Scenes::SystemRegistry::Register("Script", [this](ECS::World& w, const float dt) {
            (void)w;
            (void)dt;
            if (!m_scriptSystem || !m_scriptSystem->IsInitialized()) return;
            
            // Scripts are updated through ScriptSystem::Update which is called separately
            // This registration mainly ensures the system is tracked
        });

        // For now, the UIEventSystem is initialized like this
        // Init UI events system
        ECS::UIEventSystem::Initialize();
    }

    void Application::_onGameStart(Scenes::Scene* scene) {
        if (!scene) return;

        auto* world = &scene->GetWorld();
        auto it = Audio::AUDIO_MAP.find(world);
        if (it != Audio::AUDIO_MAP.end() && it->second) {
            it->second->OnSceneStart();
            LOG_DEBUG("AudioSystem: Notified of scene start");
        }

        // Initialize all scripts when game starts
        if (m_scriptSystem && m_scriptSystem->IsInitialized()) {
            ECS::ScriptSystem::OnStart(*world);
            LOG_DEBUG("ScriptSystem: Initialized all scripts");
        }
    }

    void Application::_onGameStop(Scenes::Scene* scene) {
        if (!scene) return;

        m_accumulator = 0.0f; // Reset accumulator on stop

        auto* world = &scene->GetWorld();
        auto it = Audio::AUDIO_MAP.find(world);
        if (it != Audio::AUDIO_MAP.end() && it->second) {
            it->second->OnSceneStop();
            LOG_DEBUG("AudioSystem: Notified of scene stop");
        }

        // Cleanup scripts when game stops
        if (m_scriptSystem && m_scriptSystem->IsInitialized()) {
            ECS::ScriptSystem::OnDestroy(*world);
            LOG_DEBUG("ScriptSystem: Cleaned up scripts");
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
        // Standalone build: always run game logic
        if (!IsInEditorMode())
            return true;
        
        // Editor build: respect play/pause state
        return m_overlay->IsGamePlaying();
    }

    void Application::_updatePhysics(ECS::World& world, bool shouldRun, bool stepRequested) {
        auto* physicsSystem = Scenes::SystemRegistry::Get("Physics");
        if (!physicsSystem) return;

        // Editor-only: Enable/disable physics based on play state
        if (IsInEditorMode()) {
            // Toggle Physics
            if (shouldRun && !Scenes::SystemRegistry::IsEnabled("Physics")) {
                Scenes::SystemRegistry::Enable("Physics");
            }
            else if (!shouldRun && Scenes::SystemRegistry::IsEnabled("Physics")) {
                Scenes::SystemRegistry::Disable("Physics");
            }

            // Toggle Lifetime
            if (shouldRun && !Scenes::SystemRegistry::IsEnabled("Lifetime")) {
                Scenes::SystemRegistry::Enable("Lifetime");
            }
            else if (!shouldRun && Scenes::SystemRegistry::IsEnabled("Lifetime")) {
                Scenes::SystemRegistry::Disable("Lifetime");
            }

            // Toggle Animation
            if (shouldRun && !Scenes::SystemRegistry::IsEnabled("Animation")) {
                Scenes::SystemRegistry::Enable("Animation");
            }
            else if (!shouldRun && Scenes::SystemRegistry::IsEnabled("Animation")) {
                Scenes::SystemRegistry::Disable("Animation");
            }
        }

        // Run physics at fixed timestep when playing
        if (shouldRun) {
            m_accumulator += Time::UnscaledDeltaTime();

            const float maxAccumulator = Time::UnscaledFixedDeltaTime() * 5.0f;
            if (m_accumulator > maxAccumulator)
                m_accumulator = maxAccumulator;

            while (m_accumulator >= Time::UnscaledFixedDeltaTime()) {
                // (*physicsSystem)(world, Time::UnscaledFixedDeltaTime());
                
                // Run script FixedUpdate during physics step
                if (m_scriptSystem && m_scriptSystem->IsInitialized()) {
                    ECS::ScriptSystem::FixedUpdate(world);
                }
                
                m_accumulator -= Time::UnscaledFixedDeltaTime();
            }
        }
        // Editor-only: Single step when paused
        else if (stepRequested) {
            (*physicsSystem)(world, Time::UnscaledFixedDeltaTime());
            if (m_overlay) m_overlay->ClearStepRequest();
        }
    }

    void Application::_updateScripts(ECS::World& world, bool shouldRun) {
        if (!shouldRun || !m_scriptSystem || !m_scriptSystem->IsInitialized())
            return;

        // Regular update
        ECS::ScriptSystem::Update(world);
        
        // Active state changes (OnEnable/OnDisable)
        ECS::ScriptSystem::UpdateActiveState(world);
        
        // Late update
        ECS::ScriptSystem::LateUpdate(world);
    }
}