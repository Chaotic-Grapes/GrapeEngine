#include "Application.h"
#include <windows.h>
#include "Input.h"
#include "Physics2D.h"
#include "Renderer2D.h"
#include "systems/WindowManager.h"
#include "ecs/Scene.h"

namespace Engine {
    bool Application::m_shouldStop = false;

    void Application::Run(Game& game, const bool consoleFlag) {
#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);
        Scene* currentScene = nullptr;
        
        while (!m_shouldStop) {
			Input::_processInput();
            auto* newScene = m_sceneManager.GetActiveScene();
            const bool isNewScene = newScene == currentScene;
            if (!isNewScene) {
                if (currentScene)
                    currentScene->Unload();

                newScene->Load();

                delete currentScene;
                currentScene = newScene;
            }

            if (currentScene) {
                currentScene->Update();
                game.OnUpdate(m_sceneManager);
                currentScene->LateUpdate();
            }
            
            for (const auto* win : WindowManager::GetWindows()) {
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
                glfwSwapBuffers(win->Handle());
            }
		}

        if (currentScene) {
            game.OnShutdown(m_sceneManager);
            currentScene->Unload();
        }

        WindowManager::DestroyAll();
    }

    void Application::Close() {
		m_shouldStop = true;
    }

    void Application::_enableConsole() {
#ifdef _WIN32
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
