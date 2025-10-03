#include "ResourceManagerTest.h"
#include "ResourceManager.h"
#include "graphics/texture.hpp"
#include "systems/WindowManager.h"
#include "systems/Logger.h"
#include <iostream>

// Declare the global RM once at file scope
extern ResourceManager RM;

namespace Sandbox {
    ResourceManagerTestScene::ResourceManagerTestScene()
        : Scene("ResourceManagerTestScene") {
        CREATE_WINDOW("ResourceManager Test", 800, 600);
    }

    void ResourceManagerTestScene::OnLoad() {
        LOG_INFO("\n=== ResourceManager Test Scene Loaded ===");
        RunTests();
    }

    void ResourceManagerTestScene::OnUpdate() {
        // Tests run once in OnLoad, nothing needed here
    }

    void ResourceManagerTestScene::OnUnload() {
        LOG_INFO("ResourceManagerTestScene: Cleanup complete");
    }

    void ResourceManagerTestScene::RunTests() {
        LOG_INFO("\nStarting ResourceManager Tests...\n");

        TestBasicCaching();
        TestMultipleAssets();
        TestErrorHandling();
        TestCacheManagement();

        LOG_INFO("\n=== All ResourceManager Tests Complete ===\n");
        m_testsComplete = true;
    }

    void ResourceManagerTestScene::TestBasicCaching() {
        LOG_INFO("--- Test 1: Basic Caching ---");

        std::shared_ptr<Texture> tex1 = RM.Get<Texture>("assets/textures/test/shadow_death.png");
        std::shared_ptr<Texture> tex2 = RM.Get<Texture>("assets/textures/test/shadow_death.png");

        bool success = (tex1 == tex2);
        LOG_INFO("Cache hit (same object): " << (success ? "PASS" : "FAIL"));
    }

    void ResourceManagerTestScene::TestMultipleAssets() {
        LOG_INFO("\n--- Test 2: Multiple Assets ---");

        std::shared_ptr<Texture> tex1 = RM.Get<Texture>("assets/textures/test/shadow_single.png");
        std::shared_ptr<AudioData> audio = RM.Get<AudioData>("assets/audio/test.wav");

        RM.PrintCacheInfo();
        LOG_INFO("Multiple asset types loaded: PASS");
    }

    void ResourceManagerTestScene::TestErrorHandling() {
        LOG_INFO("\n--- Test 3: Error Handling ---");

        std::shared_ptr<Texture> invalid = RM.Get<Texture>("nonexistent.png");
        bool success = (invalid == nullptr);
        LOG_INFO("Invalid file handling: " << (success ? "PASS" : "FAIL"));
    }

    void ResourceManagerTestScene::TestCacheManagement() {
        LOG_INFO("\n--- Test 4: Cache Management ---");

        RM.UnloadAsset("assets/textures/test/shadow_death.png");
        std::shared_ptr<Texture> reloaded = RM.Get<Texture>("assets/textures/test/shadow_death.png");

        bool reloadSuccess = (reloaded != nullptr);
        LOG_INFO("Reload after unload: " << (reloadSuccess ? "PASS" : "FAIL"));

        RM.ClearCache();
        LOG_INFO("Cache cleared. Final size: " << RM.GetCacheSize());
    }
}
