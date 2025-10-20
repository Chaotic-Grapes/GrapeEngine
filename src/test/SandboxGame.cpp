#include "Game.h"
#include "GraphicsTest.hpp"
#include "MemoryTest.h"
#include "PhysicsCollision2DTest.h"
#include "ResourceManagerTest.h"
#include "SandboxGame.h"
#include "SerializationTest.h"
#include "core/Application.h"
#include "services/WindowManager.h"
#include <iostream>
#include <memory>
#include "ECSTest.hpp"


void SandboxGame::OnStart(Scenes::SceneManager& sceneManager) {
    // Get configuration from the application
    // const auto& config = Engine::CORE->GetConfig();
    // const int windowWidth = config.WindowConfig.Width;
    // const int windowHeight = config.WindowConfig.Height;

    std::cout << "Select test scene: \n";
    //std::cout << "1. Physics & Collision 2D Test" << '\n';
    std::cout << "2. Graphics & Art Pipeline Test" << '\n';
    std::cout << "3. Serialization Check Test" << '\n';
    std::cout << "4. Memory Tracking Test" << '\n';
	std::cout << "5. Entity Component System Test" << '\n';

    int choice;
    std::cin >> choice;

    // Clear console
    printf("\033[H\033[J");

    switch (choice) {
    // case 1: { // PhysicsCollision
    //     std::cout << "Starting Physics & Collision 2D Test..." << '\n';

    //     sceneManager.AddScene(new Sandbox::PhysicsCollision2DTestScene(windowWidth, windowHeight, 7.0f));
    //     sceneManager.LoadScene("PhysicsCollision2DTestScene");
    //     break;
    // }
    case 2: {
        LOG_INFO("Starting Graphics & Art Pipeline Test...");
        size_t graphicsTest = sceneManager.AddScene(new Sandbox::GraphicsTestScene());
        sceneManager.SetActive(graphicsTest);

        break;
    }
    // case 3: {
    //     std::cout << "Starting Serialization Integrity Test..." << '\n';

    //     sceneManager.AddScene(new Sandbox::SerializationTestScene());
    //     sceneManager.LoadScene("SerializationTestScene");
    //     break;
    // }
    case 4: {
        LOG_INFO("Starting Memory Tracking Test...");
        RunMemoryTests();
        break;
    }
    case 5: {
        LOG_INFO("Starting Entity Component System Test...");
        size_t ecsTest = sceneManager.AddScene(new Sandbox::ECSTestScene());
        sceneManager.SetActive(ecsTest);

        break;
	}
    default:
        std::cout << "Invalid choice" << '\n';
        break;
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