#include "Game.h"
#include "GraphicsTest.hpp"
#include "MemoryTest.h"
#include "ResourceManagerTest.h"
#include "SandboxGame.h"
#include "SerializationTest.h"
#include "core/Application.h"
#include "services/WindowManager.h"
#include <iostream>
#include <memory>
#include "ECSTest.hpp"
#include "ScriptingTest.hpp"


void SandboxGame::OnStart(Scenes::SceneManager& sceneManager) {
    // Get configuration from the application
    // const auto& config = Engine::CORE->GetConfig();
    // const int windowWidth = config.WindowConfig.Width;
    // const int windowHeight = config.WindowConfig.Height;

    std::cout << "Select test scene: \n";
    std::cout << "1. Graphics & Art Pipeline Test" << '\n';
    std::cout << "2. Serialization Check Test" << '\n';
    std::cout << "3. Memory Tracking Test" << '\n';
	std::cout << "4. Entity Component System Test" << '\n';
	std::cout << "5. C# Scripting System Test" << '\n';

    int choice;
    std::cin >> choice;

    // Clear console
    printf("\033[H\033[J");

    switch (choice) {
        case 1: {
            LOG_INFO("Starting Graphics & Art Pipeline Test...");
            size_t graphicsTest = sceneManager.AddScene(new Sandbox::GraphicsTestScene());
            sceneManager.SetActive(graphicsTest);

            break;
        }
        case 2: {
            LOG_INFO("Starting Serialization Integrity Test...");
            size_t serializationTest = sceneManager.AddScene(new Sandbox::SerializationTestScene());
            sceneManager.SetActive(serializationTest);
            break;
        }
        case 3: {
            LOG_INFO("Starting Memory Tracking Test...");
            RunMemoryTests();
            break;
        }
        case 4: {
            LOG_INFO("Starting Entity Component System Test...");
            size_t ecsTest = sceneManager.AddScene(new Sandbox::ECSTestScene());
            sceneManager.SetActive(ecsTest);

            break;
        }
        case 5: {
            LOG_INFO("Starting C# Scripting System Test...");
            size_t scriptingTest = sceneManager.AddScene(new Sandbox::ScriptingTestScene());
            sceneManager.SetActive(scriptingTest);

            break;
        }
        default: {
            std::cout << "Invalid choice" << '\n';
            break;
        }
    }
}

void SandboxGame::OnUpdate(Scenes::SceneManager& sceneManager) {
    // Optionally do per-frame global logic here
    // Like checking input to switch scenes
    Scenes::Scene* activeScene = sceneManager.GetActive();
    if (activeScene) {
        // activeScene->GetWorld() or scene-level logic
    }
}

void SandboxGame::OnShutdown(Scenes::SceneManager& sceneManager) {
    (void)sceneManager;
    // Any global cleanup if needed
    // sceneManager.RemoveAllScenes();
    DESTROY_ALL_WINDOWS();
}