#include <iostream>
#include <memory>
#include "ResourceManager.h"
#include "graphics/texture.hpp"
#include "systems/Logger.h"

void TestResourceManager() {
    LOG_INFO("\n=== ResourceManager Tests ===");
    extern ResourceManager RM;

    // Test 1: Basic caching
    LOG_INFO("\n--- Basic Cache Test ---");
    std::shared_ptr<Texture> tex1 = RM.Get<Texture>("assets/textures/test/shadow_death.png");
    std::shared_ptr<Texture> tex2 = RM.Get<Texture>("assets/textures/test/shadow_death.png"); // Should be cache hit
    LOG_INFO("Same texture object: " << (tex1 == tex2 ? "YES" : "NO"));

    // Test 2: Multiple assets
    LOG_INFO("\n--- Multiple Assets ---");
    std::shared_ptr<Texture> different = RM.Get<Texture>("assets/textures/test/shadow_single.png");
    std::shared_ptr<AudioData> audio = RM.Get<AudioData>("assets/audio/test.wav");
    RM.PrintCacheInfo();

    // Test 3: Error handling
    LOG_INFO("\n--- Error Handling ---");
    std::shared_ptr<Texture> invalid = RM.Get<Texture>("nonexistent.png");
    LOG_INFO("Invalid file handled: " << (invalid == nullptr ? "YES" : "NO"));

    // Test 4: Cache management
    LOG_INFO("\n--- Cache Management ---");
    RM.UnloadAsset("assets/textures/test/shadow_death.png");
    std::shared_ptr<Texture> reloaded = RM.Get<Texture>("assets/textures/test/shadow_death.png");
    LOG_INFO("Reload after unload: " << (reloaded != nullptr ? "YES" : "NO"));
    RM.ClearCache();
    LOG_INFO("Final cache size: " << RM.GetCacheSize());

    LOG_INFO("\n=== Tests Complete ===");
}
