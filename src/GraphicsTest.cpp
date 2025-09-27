#include "GraphicsTest.hpp"
#include "input.h"
#include "ecs/Components.h"
#include "systems/Window.h"
#include "systems/WindowManager.h"
#include <glm/glm.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "systems/Time.h"
#include "../include/Renderer2D.h"

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
            for (EntityId id : m_TestEntities) {
                Entity e(id, &world);
                world.GetEntityManager().DestroyEntity(e);
            }
            m_TestEntities.clear();

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
    case TestType::BasicGraphics:     runBasicGraphics();  break;
    case TestType::DebugDrawing:      runDebugDrawing();   break;
    case TestType::BasicSprites:      runBasicSprites();   break;
    case TestType::BackgroundRender:  runBackground();     break;
    case TestType::SpriteScaling:     runSpriteScaling();  break;
    case TestType::SpriteRotation:    runSpriteRotation(); break;
    case TestType::SpriteAnimation:   runAnimation();      break;
    case TestType::MultiAnimation:    runMultiAnimation(); break;
    case TestType::PerformanceTest:   runBatchStress();    break;
    case TestType::FontSystem:        runFontSystem();     break;

    // ============================================================
    // PERFORMANCE / PROFILING DEBUG TESTS
    // ============================================================
    case TestType::DebugPerformance:  debugPerformance();   break;
    case TestType::SingleTextureTest: testSingleTexture();  break;
    case TestType::AnalyzeRenderer:   analyzeRenderer();    break;
    case TestType::SmallBatchTest:    testSmallBatch();     break;
    }

    if (auto* r2d = world.GetSystem<Engine::Renderer2D>()) {
        std::cout << "FPS: " << (1.0f / Time::DeltaTime())
            << " | Flushes: " << r2d->GetFlushCount()
            << std::endl;
    }
}

void GraphicsTestScene::OnUnload() {
    std::cout << "GraphicsTestScene shutting down" << std::endl;
    m_batchSprites.clear();
}

// ------------------------------------
// Stub functions (to be implemented later)
// ------------------------------------
void GraphicsTestScene::runBasicGraphics() { 
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        Entity square = CreateEntity();
        auto& tr = square.Transform();
        tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
        tr.Scale = { 100.0f, 100.0f };

        auto& sr = square.AddComponent<ShapeRenderer2D>();
        sr.Type = ShapeRenderer2D::ShapeType::Rectangle;
        sr.FillColor = { 1.0f, 0.0f, 0.0f, 1.0f };
        sr.OutlineThickness = 0.f;
        sr.OutlineColor = { 0.f, 0.f, 0.f, 0.f };

        m_TestEntities.push_back(square.GetId());
        std::cout << "Spawned BasicGraphics entity\n";
    }

    std::cout << m_TestEntities.size() << "\n";
}

void GraphicsTestScene::runDebugDrawing() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        Entity sprite = CreateEntity();
        auto& tr = sprite.Transform();
        tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };

        auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/player.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f };

        tr.Scale = {
            static_cast<float>(sr.Width),
            static_cast<float>(sr.Height)
        };

        m_TestEntities.push_back(sprite.GetId());

        if (sr.Meta && sr.Meta->collider.type == ColliderType::AABB) {
            Entity debug = CreateEntity();
            auto& dbgTr = debug.Transform();

            dbgTr.Position = {
                tr.Position.X + sr.Meta->collider.offset.x - sr.Width * 0.5f + sr.Meta->collider.size.x * 0.5f,
                tr.Position.Y + sr.Meta->collider.offset.y - sr.Height * 0.5f + sr.Meta->collider.size.y * 0.5f
            };
            dbgTr.Scale = {
                static_cast<float>(sr.Meta->collider.size.x),
                static_cast<float>(sr.Meta->collider.size.y)
            };

            auto& shape = debug.AddComponent<ShapeRenderer2D>();
            shape.Type = ShapeRenderer2D::ShapeType::Rectangle;
            shape.FillColor = { 0.f, 0.f, 0.f, 0.f };
            shape.OutlineColor = { 0.f, 1.f, 0.f, 1.f };
            shape.OutlineThickness = 2.f;

            m_TestEntities.push_back(debug.GetId());
        }

        std::cout << "Spawned DebugDrawing entities\n";
    }
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
void GraphicsTestScene::runBasicSprites() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        Entity sprite1 = CreateEntity();
        auto& tr1 = sprite1.Transform();
        tr1.Position = { m_worldWidth * 0.4f, m_worldHeight * 0.5f };
        tr1.Scale = { 256.f, 256.f };

        auto& sr1 = sprite1.AddComponent<SpriteRenderer>("assets/textures/test/player.png");
        sr1.Color = { 1.f, 1.f, 1.f, 1.f };

        m_TestEntities.push_back(sprite1.GetId());

        Entity sprite2 = CreateEntity();
        auto& tr2 = sprite2.Transform();
        tr2.Position = { m_worldWidth * 0.6f, m_worldHeight * 0.5f };
        tr2.Scale = { 512.f, 512.f };

        auto& sr2 = sprite2.AddComponent<SpriteRenderer>("assets/textures/test/fishBoy.png");
        sr2.Color = { 1.f, 1.f, 1.f, 1.f };

        m_TestEntities.push_back(sprite2.GetId());

        std::cout << "Spawned BasicSprites entities\n";
    }
}

void GraphicsTestScene::runBackground() { /* full background */ }

void GraphicsTestScene::runSpriteScaling() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        Entity sprite = CreateEntity();
        auto& tr = sprite.Transform();
        tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
        tr.Scale = { 256.f, 256.f };

        auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/player.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f };

        m_TestEntities.push_back(sprite.GetId()); // store ID for later
    }

    // Update every frame
    float scaleSpeed = 200.f;

    Entity sprite(m_TestEntities[0], &world); // reconstruct from ID
    auto& tr = sprite.Transform();

    if (Input::IsKeyDown(KEY_J)) {
        tr.Scale.X += scaleSpeed * Time::DeltaTime();
        tr.Scale.Y += scaleSpeed * Time::DeltaTime();
    }
    if (Input::IsKeyDown(KEY_K)) {
        tr.Scale.X = std::max(10.f, tr.Scale.X - scaleSpeed * Time::DeltaTime());
        tr.Scale.Y = std::max(10.f, tr.Scale.Y - scaleSpeed * Time::DeltaTime());
    }
}

void GraphicsTestScene::runSpriteRotation() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        Entity sprite = CreateEntity();
        auto& tr = sprite.Transform();
        tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
        tr.Scale = { 512.f, 512.f };

        auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/fishBoy.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f };

        m_TestEntities.push_back(sprite.GetId());
        std::cout << "Spawned SpriteRotation entity\n";
    }

    // Update every frame
    float rotationSpeed = 180.f; // degrees per second

    Entity sprite(m_TestEntities[0], &world); // reconstruct from ID
    auto& tr = sprite.Transform();

    if (Input::IsKeyDown(KEY_R)) {
        tr.Rotation += rotationSpeed * Time::DeltaTime();
        if (tr.Rotation >= 360.f) tr.Rotation -= 360.f; // keep it normalized
    }
}

void GraphicsTestScene::runAnimation() {

}

void GraphicsTestScene::runMultiAnimation() { /* switch animations */ }

void GraphicsTestScene::runBatchStress() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        const int count = 2500; // can increase if performance allows

        for (int i = 0; i < count; ++i) {
            Entity sprite = CreateEntity();
            auto& tr = sprite.Transform();

            // Randomize position inside viewport
            tr.Position = {
                static_cast<float>(rand() % static_cast<int>(m_worldWidth)),
                static_cast<float>(rand() % static_cast<int>(m_worldHeight))
            };

            // Randomize scale a little bit
            tr.Scale = {
                16.f + (rand() % 32),
                16.f + (rand() % 32)
            };

            // Randomize rotation (0–359 degrees)
            tr.Rotation = static_cast<float>(rand() % 360);

            // Add sprite renderer with a test texture
            auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/fishBoy.png");

            // Vary tint color (optional visual variety)
            sr.Color = {
                (rand() % 100) / 100.f,
                (rand() % 100) / 100.f,
                (rand() % 100) / 100.f,
                1.f
            };

            m_TestEntities.push_back(sprite.GetId());
        }

        std::cout << "Spawned " << m_TestEntities.size()
            << " sprites for Batch Stress Test\n";
    }

    // ------------------------------------
    // Per-frame updates
    // ------------------------------------
    for (EntityId id : m_TestEntities) {
        Entity e(id, &world);
        auto& tr = e.Transform();

        // Rotate continuously (degrees per second)
        float rotationSpeed = 90.0f; // tweak as needed
        tr.Rotation += rotationSpeed * Time::DeltaTime();
        if (tr.Rotation >= 360.0f) tr.Rotation -= 360.0f;
    }

    // ------------------------------------
    // Print FPS once per second
    // ------------------------------------
    static float timeAccum = 0.0f;
    static int frameCounter = 0;

    timeAccum += Time::DeltaTime();
    frameCounter++;

    if (timeAccum >= 1.0f) {
        float fps = frameCounter / timeAccum;
        std::cout << "FPS: " << fps << std::endl;

        timeAccum = 0.0f;
        frameCounter = 0;
    }
}

void GraphicsTestScene::runFontSystem() { /* render text */ }

// ============================================================
// PERFORMANCE / PROFILING DEBUG TESTS
// ============================================================

void GraphicsTestScene::debugPerformance() {
    World& world = GetWorld();
    std::cout << "\n=== PERFORMANCE DEBUGGING ===" << std::endl;
    std::cout << "1) Run with no updates (baseline rendering)" << std::endl;
    std::cout << "2) Try small batch sizes (100, 500, 1000 sprites)" << std::endl;
    std::cout << "3) Verify if all sprites use same texture" << std::endl;
    std::cout << "4) Check buffer capacity vs 2500 sprites (~10k verts, 15k indices)" << std::endl;
    std::cout << "=============================\n" << std::endl;
}

void GraphicsTestScene::testSingleTexture() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        const int count = 2500;
        const float spacingX = m_worldWidth / 50.0f;
        const float spacingY = m_worldHeight / 50.0f;

        for (int i = 0; i < count; ++i) {
            Entity sprite = CreateEntity();
            auto& tr = sprite.Transform();

            int col = i % 50;
            int row = i / 50;
            tr.Position = {
                col * spacingX + spacingX * 0.5f,
                row * spacingY + spacingY * 0.5f
            };
            tr.Scale = { 32.f, 32.f };

            // All sprites share the same texture
            auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/fishBoy.png");
            sr.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

            m_TestEntities.push_back(sprite.GetId());
        }

        std::cout << "Spawned " << count << " identical sprites (same texture & color)" << std::endl;
        return;
    }

    // No updates — measure raw rendering cost
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        std::cout << "SingleTexture FPS: " << (1.0f / Time::DeltaTime()) << std::endl;
    }
}

void GraphicsTestScene::analyzeRenderer() {
    World& world = GetWorld();
    auto* renderer2D = world.GetSystem<Engine::Renderer2D>();

    if (!renderer2D) {
        std::cout << "ERROR: Renderer2D system not found!" << std::endl;
        return;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        int flushes = renderer2D->GetFlushCount();
        std::cout << "\n=== RENDERER ANALYSIS ===" << std::endl;
        std::cout << "Flushes this frame: " << flushes << std::endl;
        if (flushes > 10) {
            std::cout << "Too many flushes! Likely texture switches or buffer overflows..." << std::endl;
        }
        else if (flushes == 1) {
            std::cout << "Single batch, bottleneck is CPU-side or GPU fillrate" << std::endl;
        }
        std::cout << "FPS: " << (1.0f / Time::DeltaTime()) << std::endl;
        std::cout << "=========================\n" << std::endl;
    }
}

void GraphicsTestScene::testSmallBatch() {
    World& world = GetWorld();

    if (m_TestEntities.empty()) {
        const int count = 100;
        for (int i = 0; i < count; ++i) {
            Entity sprite = CreateEntity();
            auto& tr = sprite.Transform();

            tr.Position = { 100.0f + (i % 10) * 64.0f, 100.0f + (i / 10) * 64.0f };
            tr.Scale = { 32.f, 32.f };

            auto& sr = sprite.AddComponent<SpriteRenderer>("assets/textures/test/fishBoy.png");
            sr.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

            m_TestEntities.push_back(sprite.GetId());
        }

        std::cout << "Spawned small batch: " << count << " sprites" << std::endl;
        return;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        float fps = (1.0f / Time::DeltaTime());
        std::cout << "100-sprite test FPS: " << fps << std::endl;
        if (fps < 100) {
            std::cout << "Even 100 sprites are slow, that means the bottleneck is NOT batch size!!!" << std::endl;
        }
    }
}
