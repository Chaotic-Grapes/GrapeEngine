#include "SandboxGame.h"
#include "Game.h"
#include <iostream>
#include "PhysicsCollision2DTest.h"
#include "GraphicsTest.hpp"
#include "GameObjectFactoryTest.h"
#include "SerializationTest.h"
#include "systems/WindowManager.h"
#include "ResourceManagerTest.h"
#include "MemoryTest.h"

void SandboxGame::OnStart(SceneManager& sceneManager) {
    std::cout << "Select test scene: \n";
    std::cout << "1. Physics & Collision 2D Test" << '\n';
    std::cout << "2. Graphics & Art Pipeline Test" << '\n';
    std::cout << "3. Serialization Check Test" << '\n';
    std::cout << "4. Memory Tracking Test" << '\n';

    int choice;
    std::cin >> choice;

    // Clear console
    printf("\033[H\033[J");

    switch (choice) {
    case 1: { // PhysicsCollision
        std::cout << "Starting Physics & Collision 2D Test..." << '\n';

        sceneManager.AddScene(new Sandbox::PhysicsCollision2DTestScene(1600, 900, 7.f));
        sceneManager.LoadScene("PhysicsCollision2DTestScene");
        break;
    }
    case 2: {
        std::cout << "Starting Graphics & Art Pipeline Test..." << '\n';

        sceneManager.AddScene(new Sandbox::GraphicsTestScene(1600, 900));
        sceneManager.LoadScene("GraphicsTestScene");

        break;
    }
        //case 3:
        //    std::cout << "ECS Component Test - TODO" << '\n';
        //    break;
    case 4: {
		std::cout << "Starting Game Object Factory Test..." << '\n';

		sceneManager.AddScene(new Sandbox::GameObjectFactoryTestScene());
		sceneManager.LoadScene("GameObjectFactoryTestScene");
        break;
	}
    case 5: {
        std::cout << "Starting Serialization Integrity Test..." << '\n';

        sceneManager.AddScene(new Sandbox::SerializationTestScene());
        sceneManager.LoadScene("SerializationTestScene");
        break;
    }
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

void SandboxGame::OnUpdate(SceneManager& sceneManager) {
    // Optionally do per-frame global logic here
    // Like checking input to switch scenes
    Scene* activeScene = sceneManager.GetActiveScene();
    if (activeScene) {
        // activeScene->GetWorld() or scene-level logic
    }
}

void SandboxGame::OnShutdown(SceneManager& sceneManager) {
    (void)sceneManager;
    // Any global cleanup if needed
    // sceneManager.RemoveAllScenes();
    DESTROY_ALL_WINDOWS();
}