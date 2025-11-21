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
#include "audio/AudioSystem.h"
#include "audio/AudioSystemRegistry.h"
#include "scene/Scene.h"
#include "scene/SystemRegistry.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/WindowManager.h"
#include "services/OverlayService.h"
#include <thread>


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
        bool configLoaded = Serialization::ConfigurationSerializer::LoadConfig("config.json", m_config);
        if (!configLoaded) {
            // Fallback: common scenario when running from build directory
            if (Serialization::ConfigurationSerializer::LoadConfig("../config.json", m_config)) {
                LOG_INFO("Loaded configuration from parent directory: ../config.json");
            }
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


#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif
		// Initialize services
		_initializeServices();
        Scenes::SystemRegistry::Disable("Physics"); // Start with physics disabled

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);

        m_lastFrameTime = glfwGetTime();
        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            const double rawDelta = frameStart - m_lastFrameTime;
            m_lastFrameTime = frameStart;
            
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
            
            // Fixed timestep accumulator for physics, gated by playback controls
            if (currentScene) {
                const bool isPlaying = (m_overlay && m_overlay->IsGamePlaying());
                const bool stepRequested = (m_overlay && m_overlay->IsStepRequested());

                auto& world = currentScene->GetWorld();
                auto* physicsSystem = Scenes::SystemRegistry::Get("Physics");

                // Enable physics when playing, disable when stopped/paused
                if (m_overlay) {
                    if (isPlaying && !Scenes::SystemRegistry::IsEnabled("Physics")) {
                        Scenes::SystemRegistry::Enable("Physics");
                    }
                    else if (!isPlaying && Scenes::SystemRegistry::IsEnabled("Physics")) {
                        Scenes::SystemRegistry::Disable("Physics");
                    }
                }

                if (isPlaying && !wasPlaying) {
                    // Just started playing
                    LOG_INFO("Game started playing");
                    _onGameStart(currentScene);
                }
                else if (!isPlaying && wasPlaying) {
                    // Just stopped/paused
                    LOG_INFO("Game stopped/paused");
                    _onGameStop(currentScene);
                }
                wasPlaying = isPlaying;
                if (physicsSystem) {
                    if (isPlaying) {
                        m_accumulator += Time::UnscaledDeltaTime();

                        // Prevent fixed delta time from deadlocking(?)
                        const float maxAccumulator = Time::UnscaledFixedDeltaTime() * 5.0f;
                        if (m_accumulator > maxAccumulator)
                            m_accumulator = maxAccumulator;

                        // Run physics at fixed timestep while playing
                        while (m_accumulator >= Time::UnscaledFixedDeltaTime()) {
                            // (*physicsSystem)(world, Time::UnscaledFixedDeltaTime());
                            m_accumulator -= Time::UnscaledFixedDeltaTime();
                        }
                    }
                    else if (stepRequested) {
                        // Run exactly one fixed-step when paused and step requested
                        (*physicsSystem)(world, Time::UnscaledFixedDeltaTime());
                        if (m_overlay) m_overlay->ClearStepRequest();
                    }
                    else {
                        // Not playing and no step: do not accumulate or run physics
                    }
                }
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

        game.OnShutdown(m_sceneManager);

        // Clean up services
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

    void Application::_initializeServices() {
        m_audio = new Services::AudioService();
        m_audio->Initialize();

        m_overlay = new Services::OverlayService(m_sceneManager);
        m_overlay->SetAudio(m_audio->Device());
		m_overlay->Initialize();

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
        
        // Render (non-static system): use a persistent instance
        Scenes::SystemRegistry::Register("Render", [](ECS::World& w, const float dt) {
            static ECS::RendererSystem s_renderer;
            s_renderer.Initialize(w);
            s_renderer.BindWorld(w);
            s_renderer.Update(w, dt);
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
    }

    void Application::_onGameStart(Scenes::Scene* scene) {
        if (!scene) return;

        auto* world = &scene->GetWorld();
        auto it = Audio::AUDIO_MAP.find(world);
        if (it != Audio::AUDIO_MAP.end() && it->second) {
            it->second->OnSceneStart();
            LOG_DEBUG("AudioSystem: Notified of scene start");
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
}