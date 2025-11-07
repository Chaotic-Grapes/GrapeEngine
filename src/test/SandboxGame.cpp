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
#include "LevelEditorTest.h"
#include "services/OverlayService.h"

void SandboxGame::OnStart(Scenes::SceneManager& sceneManager) {
    std::cout << "Select test scene: \n";
    std::cout << "2. Graphics & Art Pipeline Test" << '\n';
    std::cout << "3. Serialization Check Test" << '\n';
    std::cout << "4. Memory Tracking Test" << '\n';
	std::cout << "5. Entity Component System Test" << '\n';
    std::cout << "6. Open Level Editor" << '\n';

    int choice;
    std::cin >> choice;

    printf("\033[H\033[J");

    switch (choice) {
    case 2: {
        LOG_INFO("Starting Graphics & Art Pipeline Test...");
        size_t graphicsTest = sceneManager.AddScene(new Sandbox::GraphicsTestScene());
        sceneManager.SetActive(graphicsTest);
        break;
    }
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
    case 6: {
        LOG_INFO("Starting Level Editor (scene-less startup)...");
        const auto& config = Engine::CORE->GetConfig();
        CREATE_WINDOW("Level Editor", config.WindowConfig.Width, config.WindowConfig.Height);
        if (auto* overlay = Services::OverlayService::Get()) {
            overlay->EnableLevelEditorForScene(nullptr);
        }
        break;
    }
    default:
        std::cout << "Invalid choice" << '\n';
        break;
    }
}

void SandboxGame::OnUpdate(Scenes::SceneManager& sceneManager) {
    Scenes::Scene* activeScene = sceneManager.GetActive();
    if (activeScene) {
        // activeScene->GetWorld() or scene-level logic
    }
}

void SandboxGame::OnShutdown(Scenes::SceneManager& sceneManager) {
    (void)sceneManager;
    DESTROY_ALL_WINDOWS();
}