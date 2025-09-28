#include <iostream>
#include <memory>
#include "ResourceManager.h"
#include "graphics/texture.hpp"

void TestResourceManager() {
    std::cout << "\n=== ResourceManager Tests ===" << std::endl;
    extern ResourceManager RM;

    // Test 1: Basic caching
    std::cout << "\n--- Basic Cache Test ---" << std::endl;
    std::shared_ptr<Texture> tex1 = RM.Get<Texture>("assets/textures/test/shadow_death.png");
    std::shared_ptr<Texture> tex2 = RM.Get<Texture>("assets/textures/test/shadow_death.png"); // Should be cache hit
    std::cout << "Same texture object: " << (tex1 == tex2 ? "YES" : "NO") << std::endl;

    // Test 2: Multiple assets
    std::cout << "\n--- Multiple Assets ---" << std::endl;
    std::shared_ptr<Texture> different = RM.Get<Texture>("assets/textures/test/shadow_single.png");
    std::shared_ptr<RAudio> audio = RM.Get<RAudio>("assets/audio/test.wav");
    RM.PrintCacheInfo();

    // Test 3: Error handling
    std::cout << "\n--- Error Handling ---" << std::endl;
    std::shared_ptr<Texture> invalid = RM.Get<Texture>("nonexistent.png");
    std::cout << "Invalid file handled: " << (invalid == nullptr ? "YES" : "NO") << std::endl;

    // Test 4: Cache management
    std::cout << "\n--- Cache Management ---" << std::endl;
    RM.UnloadAsset("assets/textures/test/shadow_death.png");
    std::shared_ptr<Texture> reloaded = RM.Get<Texture>("assets/textures/test/shadow_death.png");
    std::cout << "Reload after unload: " << (reloaded != nullptr ? "YES" : "NO") << std::endl;

    RM.ClearCache();
    std::cout << "Final cache size: " << RM.GetCacheSize() << std::endl;
    std::cout << "\n=== Tests Complete ===" << std::endl;
}
