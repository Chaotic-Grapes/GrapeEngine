#include "GraphicsTest.hpp"
#include <iostream>
#include "input.h"
#include "ecs/Components.h"
#include "systems/Window.h"
#include "systems/WindowManager.h"

using namespace Sandbox;
using Component::SpriteRenderer;
using Component::Transform;

GraphicsTestScene::GraphicsTestScene(int width, int height) : Scene("GraphicsTestScene") {
    CREATE_WINDOW("Graphics Test", width, height);
    m_worldWidth  = static_cast<float>(width);
    m_worldHeight = static_cast<float>(height);
    m_currentTest = TestType::BasicGraphics;
}

void GraphicsTestScene::OnLoad() {
    World& world = GetWorld();
    std::cout << "GraphicsTestScene initialized" << std::endl;
}

void GraphicsTestScene::OnUpdate() {
    // Cycle through test types with G
    if (Input::IsKeyPressed(GLFW_KEY_G)) {
        int current = static_cast<int>(m_currentTest);
        current++;

        if (current > static_cast<int>(TestType::FontSystem)) {
            current = static_cast<int>(TestType::BasicGraphics);
        }

        m_currentTest = static_cast<TestType>(current);
        std::cout << "Switched to test " << current << std::endl;
    }

    World& world = GetWorld();

    // Run the current test
    switch (m_currentTest) {
    case TestType::BasicGraphics:     runBasicGraphics(world);  break;
    case TestType::DebugDrawing:      runDebugDrawing(world);   break;
    case TestType::BasicSprites:      runBasicSprites(world);   break;
    case TestType::BackgroundRender:  runBackground(world);     break;
    case TestType::SpriteScaling:     runSpriteScaling(world);  break;
    case TestType::SpriteRotation:    runSpriteRotation(world); break;
    case TestType::SpriteAnimation:   runAnimation(world);      break;
    case TestType::MultiAnimation:    runMultiAnimation(world); break;
    case TestType::PerformanceTest:   runBatchStress(world);    break;
    case TestType::FontSystem:        runFontSystem(world);     break;
    }
}

void GraphicsTestScene::OnUnload() {
    std::cout << "GraphicsTestScene shutting down" << std::endl;
    m_batchSprites.clear();
}

// ------------------------------------
// Stub functions (to be implemented later)
// ------------------------------------
void GraphicsTestScene::runBasicGraphics(World& world) { /* draw something simple */ }
void GraphicsTestScene::runDebugDrawing(World& world) { /* lines, circles, rects, polygons */ }
void GraphicsTestScene::runBasicSprites(World& world) { /* two different sprites */ }
void GraphicsTestScene::runBackground(World& world) { /* full background */ }
void GraphicsTestScene::runSpriteScaling(World& world) { /* scale with input */ }
void GraphicsTestScene::runSpriteRotation(World& world) { /* rotate with input */ }
void GraphicsTestScene::runAnimation(World& world) { /* play frames */ }
void GraphicsTestScene::runMultiAnimation(World& world) { /* switch animations */ }
void GraphicsTestScene::runBatchStress(World& world) { /* spawn 2500+ sprites/gameobj */ }
void GraphicsTestScene::runFontSystem(World& world) { /* render text */ }