#include "SandboxGame.h"
#include "Game.h"
#include <iostream>
#include "PhysicsCollision2DTest.h"
#include "GraphicsTest.hpp"
#include "systems/WindowManager.h"
#include "ResourceManagerTest.h"

void SandboxGame::OnStart(SceneManager& sceneManager) {
    std::cout << "Select test scene: \n";
    std::cout << "1. Physics & Collision 2D Test" << '\n';
    std::cout << "2. GRAPHICS AND ART PIPELINE TEST" << '\n';
    //std::cout << "3. ECS Component Test" << '\n';
    std::cout << "3. Resource Manager Test" << '\n';

    int choice;
    std::cin >> choice;

    // Clear console
    // Not thread-safe but does it matter?
    system("cls");

    switch (choice) {
    case 1: { // PhysicsCollision
        std::cout << "Starting Physics & Collision 2D Test..." << '\n';

        sceneManager.AddScene(new Sandbox::PhysicsCollision2DTestScene(1600, 900, 7.f));
        sceneManager.LoadScene("PhysicsCollision2DTestScene");
        break;
    }
    case 2: {
        std::cout << "Starting GRAPHICS AND ART PIPELINE TEST..." << '\n';

        sceneManager.AddScene(new Sandbox::GraphicsTestScene(1600, 900));
        sceneManager.LoadScene("GraphicsTestScene");

        break;
    }
        //case 3:
        //    std::cout << "ECS Component Test - TODO" << '\n';
        //    break;
    case 3: {
        std::cout << "Starting ResourceManager Test..." << '\n';
        // Create a minimal scene to get OpenGL context
        sceneManager.AddScene(new Sandbox::PhysicsCollision2DTestScene(1600, 900, 7.f));
        sceneManager.LoadScene("PhysicsCollision2DTestScene");

        TestResourceManager();
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