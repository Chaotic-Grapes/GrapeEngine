#include "Game.h"
#include "GraphicsTest.hpp"
#include "MemoryTest.h"
#include "ResourceManagerTest.h"
#include "SandboxGame.h"
#include "SerializationTest.h"
#include "core/Application.h"
#include "scene/TestSceneManager.h"
#include "services/WindowManager.h"
#include <iostream>
#include <memory>
#include "ECSTest.hpp"
#include "ScriptingTest.hpp"
#include "PhysicsTest.h"

// Static test scene manager for engine tests
static Scenes::TestSceneManager g_testSceneManager;

void SandboxGame::OnStart(Scenes::SceneManager& sceneManager) {
    (void)sceneManager; // Not used for test scenes
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
    std::cout << "6. Physics & Collision System Test" << '\n';

    int choice;
    std::cin >> choice;

    // Clear console
    printf("\033[H\033[J");

    switch (choice) {
        case 1: {
            LOG_INFO("Starting Graphics & Art Pipeline Test...");
            size_t graphicsTest = g_testSceneManager.AddScene(new Sandbox::GraphicsTestScene());
            g_testSceneManager.SetActive(graphicsTest);

            break;
        }
        case 2: {
            LOG_INFO("Starting Serialization Integrity Test...");
            size_t serializationTest = g_testSceneManager.AddScene(new Sandbox::SerializationTestScene());
            g_testSceneManager.SetActive(serializationTest);
            break;
        }
        case 3: {
            LOG_INFO("Starting Memory Tracking Test...");
            RunMemoryTests();
            break;
        }
        case 4: {
            LOG_INFO("Starting Entity Component System Test...");
            size_t ecsTest = g_testSceneManager.AddScene(new Sandbox::ECSTestScene());
            g_testSceneManager.SetActive(ecsTest);

            break;
        }
        case 5: {
            LOG_INFO("Starting C# Scripting System Test...");
            size_t scriptingTest = g_testSceneManager.AddScene(new Sandbox::ScriptingTestScene());
            g_testSceneManager.SetActive(scriptingTest);

            break;
        }
        case 6: { 
            LOG_INFO("Starting Physics & Collision System Test...");
            size_t PCTest = g_testSceneManager.AddScene(new Sandbox::PhysicsTestScene());
            g_testSceneManager.SetActive(PCTest);

            break;
        }
        default: {
            std::cout << "Invalid choice" << '\n';
            break;
        }
    }
}

void SandboxGame::OnUpdate(Scenes::SceneManager& sceneManager) {
    (void)sceneManager; // Not used for test scenes
    
    // Update test scene manager
    g_testSceneManager.Update();
}

void SandboxGame::OnShutdown(Scenes::SceneManager& sceneManager) {
    (void)sceneManager;
    // Any global cleanup if needed
    DESTROY_ALL_WINDOWS();
}