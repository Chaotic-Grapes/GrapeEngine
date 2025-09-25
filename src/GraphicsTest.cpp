#include "GraphicsTest.hpp"
#include "input.h"
#include "ecs/Components.h"
#include "systems/Window.h"
#include "systems/WindowManager.h"
#include <glm/glm.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace Sandbox;
using Component::LineRenderer;
using Component::ShapeRenderer2D;
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
    std::cout << "\nGraphicsTestScene initialized" << std::endl;
}

void GraphicsTestScene::OnUpdate() {
    World& world = GetWorld();

    // Cycle through test types with G
    if (Input::IsKeyDown(KEY_G)) {
        if (!m_gHandled) {
            int current = static_cast<int>(m_currentTest);
            current++;

            if (current > static_cast<int>(TestType::FontSystem)) {
                current = static_cast<int>(TestType::BasicGraphics);
            }

            // cleanup old test entities
            for (EntityId id : m_activeTestEntities) {
                Entity e(id, &world);
                world.GetEntityManager().DestroyEntity(e);
            }
            m_activeTestEntities.clear();

            m_currentTest = static_cast<TestType>(current);
            std::cout << "Switched to test " << current << std::endl;

            m_gHandled = true; // mark handled until key released
        }
    }
    else {
        m_gHandled = false; // reset when key is released
    }

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
void GraphicsTestScene::runBasicGraphics(World& world) { 
    Entity square = world.CreateEntity();
    auto& tr = square.Transform();
    tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    tr.Scale = { 100.0f, 100.0f };

    auto& sr = square.AddComponent<ShapeRenderer2D>();
    sr.Type = ShapeRenderer2D::ShapeType::Rectangle;
    sr.FillColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    sr.OutlineThickness = 0.f;
    sr.OutlineColor = { 0.f, 0.f, 0.f, 0.f };  // transparent

    m_activeTestEntities.push_back(square.GetId());
}

void GraphicsTestScene::runDebugDrawing(World& world) {
    glm::vec2 center{ m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 halfExtents{ 150.f, 100.f };

    Entity aabbEntity = world.CreateEntity();
    auto& tr = aabbEntity.Transform();
    tr.Position = { center.x, center.y };
    tr.Scale = { halfExtents.x * 2.f, halfExtents.y * 2.f };

    auto& shape = aabbEntity.AddComponent<ShapeRenderer2D>();
    shape.Type = ShapeRenderer2D::ShapeType::Rectangle;
    shape.FillColor = { 0.f, 0.f, 0.f, 0.f };
    shape.OutlineColor = { 0.f, 1.f, 0.f, 1.f };
    shape.OutlineThickness = 2.f;

    m_activeTestEntities.push_back(aabbEntity.GetId());
}

//------------------------------------------------------------------------------
/*!
\brief
    Test case for rendering basic sprites using the ECS + Renderer2D pipeline.

    This test spawns two entities, each with a SpriteRenderer component bound
    to a PNG texture ("player.png" and "IDLE.png"). It verifies that textures
    can be loaded from disk, bound to OpenGL, and drawn as quads with correct
    positions, scaling, and tint colors.

\details
    By default, OpenGL uses GL_LINEAR sampling, which blends neighboring
    texels for smoother scaling. However, when sprites contain transparency
    (e.g., alpha cutouts with white padding), linear filtering can cause
    "haloing" or whitish edges because transparent pixels still contribute
    their RGB values during interpolation.

    Switching to GL_NEAREST sampling resolves this issue for 2D sprites:
      - GL_LINEAR => Smooth scaling, but edges may bleed unwanted colors.
      - GL_NEAREST => Crisp pixel edges, no bleeding, but blocky when scaled.

    For pixel art and sprite-based rendering, GL_NEAREST is usually preferred.
    For high-resolution textures or backgrounds where smooth scaling is desired,
    GL_LINEAR may still be appropriate.

\test
    Expected outcome: Two 256×256 sprites appear centered at 40% and 60% of the
    screen width, showing their PNG textures accurately without color distortion
    at the edges.
*/
//------------------------------------------------------------------------------
void GraphicsTestScene::runBasicSprites(World& world) {
    // Sprite 1
    {
        Entity sprite1 = CreateEntity();
        auto& tr = sprite1.Transform();
        tr.Position = { m_worldWidth * 0.4f, m_worldHeight * 0.5f };
        tr.Scale = { 256.f, 256.f }; // size of the sprite

        auto& sr = sprite1.AddComponent<SpriteRenderer>("assets/textures/samurai-test/player.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f }; // true to PNG

        m_activeTestEntities.push_back(sprite1.GetId());
    }

    // Sprite 2
    {
        Entity sprite2 = world.CreateEntity();
        auto& tr = sprite2.Transform();
        tr.Position = { m_worldWidth * 0.6f, m_worldHeight * 0.5f };
        tr.Scale = { 256.f, 256.f };

        auto& sr = sprite2.AddComponent<SpriteRenderer>("assets/textures/samurai-test/IDLE.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f }; // true to PNG

        m_activeTestEntities.push_back(sprite2.GetId());
    }

    std::cout << "Created 2 basic sprites\n";
}

void GraphicsTestScene::runBackground(World& world) { /* full background */ }
void GraphicsTestScene::runSpriteScaling(World& world) { /* scale with input */ }
void GraphicsTestScene::runSpriteRotation(World& world) { /* rotate with input */ }
void GraphicsTestScene::runAnimation(World& world) { /* play frames */ }
void GraphicsTestScene::runMultiAnimation(World& world) { /* switch animations */ }
void GraphicsTestScene::runBatchStress(World& world) { /* spawn 2500+ sprites/gameobj */ }
void GraphicsTestScene::runFontSystem(World& world) { /* render text */ }