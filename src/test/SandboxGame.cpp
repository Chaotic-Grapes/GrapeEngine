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

    int choice;
    std::cin >> choice;

    // Clear console
    printf("\033[H\033[J");

    // Add scenes
    size_t graphicsTest = sceneManager.AddScene(new Sandbox::GraphicsTestScene());
    // size_t serializationTest = sceneManager.AddScene(std::make_unique<Sandbox::SerializationTestScene>());

    switch (choice) {
    // case 1: { // PhysicsCollision
    //     std::cout << "Starting Physics & Collision 2D Test..." << '\n';

    //     sceneManager.AddScene(new Sandbox::PhysicsCollision2DTestScene(windowWidth, windowHeight, 7.0f));
    //     sceneManager.LoadScene("PhysicsCollision2DTestScene");
    //     break;
    // }
    case 2: {
        LOG_INFO("Starting Graphics & Art Pipeline Test...");
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
        std::cout << "Starting Memory Tracking Test..." << '\n';
        RunMemoryTests();
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