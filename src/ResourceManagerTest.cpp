#include <iostream>
#include <vector>
#include <memory>
#include "ResourceManager.h"
#include "graphics/texture.hpp"
#include "graphics/stb_image.h"
#include "systems/Time.h"
#include <GLFW/glfw3.h>

void TestResourceManagerAdvanced() {
    std::cout << "\n=== ResourceManager Tests ===" << std::endl;

    // Test 1: Cache Hit Performance
    // Measure how quickly ResourceManager can handle repeated requests for the same texture
    // After the first load, all subsequent requests should be cache hits
    std::cout << "\n--- Test 1: Cache Performance ---" << std::endl;
    double startTime = glfwGetTime();

    // Load same texture 100 times (first call loads from disk, rest should use cache)
    std::vector<std::unique_ptr<Texture>> textures;
    for (int i{}; i < 100; i++) {
        textures.push_back(std::make_unique<Texture>("assets/textures/test/shadow_death.png"));
    }

    double endTime = glfwGetTime();
    double durationMicroseconds = (endTime - startTime) * 1'000'000.0; // Convert to microseconds
    std::cout << "Loaded 100 textures in " << durationMicroseconds << " microseconds" << std::endl;
    std::cout << "Cache working if time is low (should be < 50ms)" << std::endl;

    // Test 2: Mixed Asset Loading
    // Verify that ResourceManager can handle different asset types and maintain separate caches for textures and audio files
    std::cout << "\n--- Test 2: Mixed Asset Types ---" << std::endl;
    extern ResourceManager RM; // Access global instance

    // Load different texture files to populate cache with multiple entries
    std::shared_ptr<RTexture> tex1 = RM.Get<RTexture>("assets/textures/test/shadow_death.png");
    std::shared_ptr<RTexture> tex2 = RM.Get<RTexture>("assets/textures/test/shadow_single.png");

    // Display current cache statistics
    RM.PrintCacheInfo();

    // Test 3: Invalid Files
    // Ensure ResourceManager handles missing or invalid files by returning nullptr instead of crashing or causing memory leaks
    std::cout << "\n--- Test 3: Error Handling ---" << std::endl;
    std::shared_ptr<RTexture> badTex = RM.Get<RTexture>("nonexistent.png");
    std::shared_ptr<RAudio> badAudio = RM.Get<RAudio>("fake.wav");
    std::cout << "Bad texture valid: " << (badTex ? "YES" : "NO") << std::endl;
    std::cout << "Bad audio valid: " << (badAudio ? "YES" : "NO") << std::endl;

    // Test 4: Cache Management
    // Verify that we can manually remove assets from cache and that they can be reloaded on demand
    std::cout << "\n--- Test 4: Cache Management ---" << std::endl;
    std::cout << "Cache size before: " << RM.GetCacheSize() << std::endl;

    // Remove a specific asset from cache
    RM.UnloadAsset("assets/textures/test/shadow_death.png");
    std::cout << "Cache size after unload: " << RM.GetCacheSize() << std::endl;

    // Try to get unloaded asset (should trigger reload from disk)
    std::shared_ptr<RTexture> reloaded = RM.Get<RTexture>("assets/textures/test/shadow_death.png");
    std::cout << "Reloaded texture valid: " << (reloaded ? "YES" : "NO") << std::endl;

    // Test 5: Memory Usage Simulation
    // Test that multiple shared_ptr references to the same cached asset don't cause memory issues
    std::cout << "\n--- Test 5: Memory Stress Test ---" << std::endl;
    std::vector<std::shared_ptr<RTexture>> textureRefs;

    // Load many different textures to test cache with multiple assets
    std::vector<std::string> testFiles = {
        "assets/textures/test/shadow_death.png",
        "assets/textures/test/shadow_single.png",
        "assets/textures/samurai-test/player.png",
        "assets/textures/samurai-test/IDLE.png"
    };

    // Create multiple references to each texture to simulate game objects
    // sharing the same cached resources
    for (const std::string& file : testFiles) {
        for (int i{}; i < 10; i++) {
            std::shared_ptr<RTexture> tex = RM.Get<RTexture>(file);
            if (tex) textureRefs.push_back(tex);
        }
    }

    std::cout << "Created " << textureRefs.size() << " texture references" << std::endl;
    std::cout << "Actual cached textures: " << RM.GetCacheSize() << std::endl;
    // This should pass because shared_ptr allows multiple references to same cached object
    std::cout << "Memory test: " << (textureRefs.size() > RM.GetCacheSize() ? "PASS" : "FAIL") << std::endl;

    // Test 6: Asset Information Validation
    // Verify that ResourceManager correctly extracts and stores metadata from loaded files (dimensions, channels, etc.)
    std::cout << "\n--- Test 6: Asset Information ---" << std::endl;
    std::shared_ptr<RTexture> testTex = RM.Get<RTexture>("assets/textures/test/shadow_death.png");
    if (testTex) {
        std::cout << "Texture dimensions: " << testTex->Width << "x" << testTex->Height << std::endl;
        std::cout << "Channels: " << testTex->Channels << std::endl;
        std::cout << "Path: " << testTex->Path << std::endl;
        std::cout << "Valid dimensions: " << (testTex->Width > 0 && testTex->Height > 0 ? "YES" : "NO") << std::endl;
    }

    // Cleanup: clear all references and cache to reset for next test
    textureRefs.clear();
    RM.ClearCache();
    std::cout << "\nCache cleared. Final size: " << RM.GetCacheSize() << std::endl;

    std::cout << "\n=== All ResourceManager Tests Complete ===" << std::endl;
}

// Performance comparison test
// Compare the performance of direct stb_image loading versus ResourceManager caching system
void CompareResourceManagerVsDirect() {
    std::cout << "\n=== Performance Comparison ===\n" << std::endl;

    const int ITERATIONS = 50;
    const std::string testFile = "assets/textures/test/shadow_death.png";

    // Test direct loading (without ResourceManager)
    double start1 = glfwGetTime();
    for (int i{}; i < ITERATIONS; i++) {
        int w, h, c;
        unsigned char* data = stbi_load(testFile.c_str(), &w, &h, &c, 0);
        if (data) stbi_image_free(data);
    }
    double end1 = glfwGetTime();
    double directTimeMicroseconds = (end1 - start1) * 1000000.0;

    // Test ResourceManager loading
    // First call loads from disk and caches, subsequent calls use cache
    extern ResourceManager RM;
    double start2 = glfwGetTime();
    for (int i{}; i < ITERATIONS; i++) {
        std::shared_ptr<RTexture> tex = RM.Get<RTexture>(testFile);
    }
    double end2 = glfwGetTime();
    double cachedTimeMicroseconds = (end2 - start2) * 1000000.0;

    std::cout << "Direct loading (" << ITERATIONS << "x): " << directTimeMicroseconds << " microseconds" << std::endl;
    std::cout << "ResourceManager (" << ITERATIONS << "x): " << cachedTimeMicroseconds << " microseconds" << std::endl;
    std::cout << "Speedup: " << (directTimeMicroseconds / cachedTimeMicroseconds) << "x faster" << std::endl;

    // Clean up cache for next test
    RM.ClearCache();
}

// Edge case tests
// Test how ResourceManager handles unusual or problematic input
void TestEdgeCases() {
    extern ResourceManager RM;

    // Test empty filename (should fail)
    std::shared_ptr<RTexture> empty = RM.Get<RTexture>("");

    // Test extremely long path (should fail)
    std::string longPath(1000, 'a');
    longPath += ".png";
    std::shared_ptr<RTexture> longTex = RM.Get<RTexture>(longPath);

    // Test path with navigation characters: should work but create separate cache entry
    // This creates a different cache key than the direct path due to string difference
    std::shared_ptr<RTexture> special = RM.Get<RTexture>("assets/textures/test/../test/shadow_death.png");
}
