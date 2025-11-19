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
#include "scene/Scene.h"
#include "scene/SystemRegistry.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/WindowManager.h"
#include "services/OverlayService.h"
#include <thread>
#include "../engine/audio/AudioSystem.h"

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    void Application::Run(Game& game, const bool consoleFlag) {
        Time::_initialize();

        // Set global pointer to this application instance
        CORE = this;

        // Initialize crash dumping system
        Grape_Engine::CrashDumping::Initialize();
        Grape_Engine::CrashDumping::SetProgramName("GrapeEngine");
        Grape_Engine::CrashDumping::SetDumpCreateState(true);

        // Load configuration first (try working dir, then parent dir fallback)
        bool configLoaded = Serialization::ConfigurationSerializer::LoadConfig("config.json", m_config);
        if (!configLoaded) {
            // Fallback: common scenario when running from build directory
            if (Serialization::ConfigurationSerializer::LoadConfig("../config.json", m_config)) {
                LOG_INFO("Loaded configuration from parent directory: ../config.json");
            }
        }

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

        // Register core ECS systems once (Physics at fixed step, others variable)
        // Physics
        Scenes::SystemRegistry::Register("Physics", [](ECS::World& w, float dt) {
            ECS::PhysicsSystem::Update(w, dt);
        });
        // Animation
        Scenes::SystemRegistry::Register("Animation", [](ECS::World& w, float dt) {
            ECS::AnimationSystem::Update(w, dt);
        });
        // Render (non-static system): use a persistent instance
        Scenes::SystemRegistry::Register("Render", [](ECS::World& w, float dt) {
            static ECS::RendererSystem s_renderer;
            s_renderer.Initialize(w);
            s_renderer.BindWorld(w);
            s_renderer.Update(w, dt);
        });
        Scenes::SystemRegistry::Register("Audio", [](ECS::World& w, float dt) {
            // Grab the app & Serivce 
            auto* app = Engine::CORE;
            auto* svc = app ? app->GetAudioService() : nullptr;
            if (!svc) return;

            // Create/refresh a persistent AudioSystem bound to this world
            static ECS::World* s_boundWorld = nullptr;
            static std::unique_ptr<AudioSystem> s_audioSystem;

            if (!s_audioSystem || s_boundWorld != &w) {
                s_audioSystem = std::make_unique<AudioSystem>(w, *svc);
                s_boundWorld = &w;
            }

            s_audioSystem->Update(dt);
            });

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);

        m_lastFrameTime = glfwGetTime();
        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            const double rawDelta = frameStart - m_lastFrameTime;
            m_lastFrameTime = frameStart;
            
            Time::_update(rawDelta, frameStart);
            Profiler::UpdateTime();

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

                if (physicsSystem) {
                    if (isPlaying) {
                        m_accumulator += Time::UnscaledDeltaTime();

                        // Prevent fixed delta time from deadlocking(?)
                        const float maxAccumulator = Time::UnscaledFixedDeltaTime() * 5.0f;
                        if (m_accumulator > maxAccumulator)
                            m_accumulator = maxAccumulator;

                        // Run physics at fixed timestep while playing
                        while (m_accumulator >= Time::UnscaledFixedDeltaTime()) {
                            (*physicsSystem)(world, Time::UnscaledFixedDeltaTime());
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

    void Application::_initializeServices() {
        m_audio = new Services::AudioService();
        m_audio->Initialize();

		m_overlay = new Services::OverlayService(m_sceneManager);
        m_overlay->SetAudio(m_audio->Device());
		m_overlay->Initialize();
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