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
        static double lastTime = Time::ElapsedTime();
        static int frameCount = 0;

        frameCount++;
        double now = Time::ElapsedTime();

        if (now - lastTime >= 1.0) {
            double elapsed = now - lastTime;
            double fps = frameCount / elapsed;

            std::cout << "FPS: " << fps
                << " | Flushes: " << r2d->GetFlushCount()
                << std::endl;

            frameCount = 0;
            lastTime = now; // reset baseline
        }
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
}

void GraphicsTestScene::runDebugDrawing() {
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
    Important note for myself and whoever uses this:
    Test case for rendering basic sprites using the ECS + Renderer2D pipeline.

    This test spawns two entities, each with a SpriteRenderer component bound
    to a PNG texture ("player.png" and "fishBoy.png"). It verifies that textures
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

void GraphicsTestScene::runBackground() {
    if (m_TestEntities.empty()) {
        // Create an entity for the background
        Entity bg = CreateEntity();
        auto& tr = bg.Transform();

        // Center it in the world
        tr.Position = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };

        // Scale to cover the entire viewport
        tr.Scale = { m_worldWidth, m_worldHeight };

        // Add a SpriteRenderer component bound to johnPork.png
        auto& sr = bg.AddComponent<SpriteRenderer>("assets/textures/test/johnPork.png");
        sr.Color = { 1.f, 1.f, 1.f, 1.f }; // no tint

        // Store the entity ID so it persists across frames
        m_TestEntities.push_back(bg.GetId());

        std::cout << "Background loaded: johnPork.png covering "
            << m_worldWidth << "x" << m_worldHeight << "\n";
    }
}

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
        tr.Scale.X += scaleSpeed * Time::FixedDeltaTime();
        tr.Scale.Y += scaleSpeed * Time::FixedDeltaTime();
    }
    if (Input::IsKeyDown(KEY_K)) {
        tr.Scale.X = std::max(10.f, tr.Scale.X - scaleSpeed * Time::FixedDeltaTime());
        tr.Scale.Y = std::max(10.f, tr.Scale.Y - scaleSpeed * Time::FixedDeltaTime());
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
        tr.Rotation += rotationSpeed * Time::FixedDeltaTime();
        if (tr.Rotation >= 360.f) tr.Rotation -= 360.f; // keep it normalized
    }
}

void GraphicsTestScene::runAnimation() {
    World& world = GetWorld();
    auto* r2d = world.GetSystem<Engine::Renderer2D>();
    if (!r2d) return;
    auto* renderer = r2d->GetRenderer();
    auto* shader = r2d->GetShader();

    // Static variables for animation state
    static bool initialized = false;
    static std::unique_ptr<SpriteAnimation> anim;
    static std::unique_ptr<Texture> animTexture; // Keep texture alive

    // Initialize animation once
    if (!initialized) {
        try {
            animTexture = std::make_unique<Texture>("assets/textures/test/FishfolkSheet.png");
            GLuint texId = animTexture->ID();
            if (texId == 0) { std::cout << "ERROR: Failed to load animation texture!\n"; return; }

            // Use the sheet's real frame size
            int frameWidth = 32;
            int frameHeight = 32;

            anim = std::make_unique<SpriteAnimation>(
                texId, frameWidth, frameHeight, animTexture->Width(), animTexture->Height());
            anim->setFPS(8.0f);

            // tell the animation system how many frames each row really has
            // Example: your sheet has 5 rows, with 8, 6, 4, 8, 2 frames respectively
            anim->setRowFrameCounts({ 16, 8, 8, 8, 8 });

            // Choose which row or frames to animate
            anim->setRow(0);

            initialized = true;
            std::cout << "Animation initialized - Sheet: "
                << animTexture->Width() << "x" << animTexture->Height()
                << ", Frame: " << frameWidth << "x" << frameHeight << "\n";
        }
        catch (const std::exception& e) {
            std::cout << "ERROR initializing animation: " << e.what() << "\n";
            return;
        }
    }

    if (!anim || !animTexture) {
        std::cout << "Animation not properly initialized!\n";
        return;
    }

    // Use the Time system's delta time directly
    float deltaTime = Time::FixedDeltaTime();

    // Update animation and get current frame
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f }; // Scale up for visibility

    Sprite currentSprite = anim->play(pos, size, deltaTime);

    // Submit to renderer
    shader->use();
    shader->setMat4("uProjection", r2d->GetProjection());
    renderer->beginFrame();
    renderer->submitSprite(currentSprite);
    renderer->endFrame();

    // Debug info (print occasionally)
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 120 == 0) { // Every 2 seconds at 60fps
        std::cout << "Animation running - Current frame at: "
            << currentSprite.pos.x << ", " << currentSprite.pos.y << "\n";
    }
}

void GraphicsTestScene::runMultiAnimation() {
    World& world = GetWorld();
    auto* r2d = world.GetSystem<Engine::Renderer2D>();
    if (!r2d) return;
    auto* renderer = r2d->GetRenderer();
    auto* shader = r2d->GetShader();

    // Persistent animation state
    static bool initialized = false;
    static std::unique_ptr<Texture> animTexture;
    static std::unique_ptr<SpriteAnimation> idleAnim;
    static std::unique_ptr<SpriteAnimation> jumpAnim;
    static SpriteAnimation* currentAnim = nullptr; // pointer to active animation

    if (!initialized) {
        try {
            animTexture = std::make_unique<Texture>("assets/textures/test/FishfolkSheet.png");
            GLuint texId = animTexture->ID();
            if (texId == 0) {
                std::cout << "ERROR: Failed to load animation texture!\n";
                return;
            }

            int frameWidth = 32;
            int frameHeight = 32;
            int texWidth = animTexture->Width();
            int texHeight = animTexture->Height();

            // Build idle animation (row 0)
            idleAnim = std::make_unique<SpriteAnimation>(texId, frameWidth, frameHeight, texWidth, texHeight);
            idleAnim->setFPS(6.0f);
            idleAnim->setRowFrameCounts({ 16, 8, 8, 8, 8 });
            idleAnim->setRow(4);

            // Build jump animation (row 3)
            jumpAnim = std::make_unique<SpriteAnimation>(texId, frameWidth, frameHeight, texWidth, texHeight);
            jumpAnim->setFPS(10.0f);
            jumpAnim->setRowFrameCounts({ 16, 8, 8, 8, 8 });
            jumpAnim->setRow(3);

            // Start with idle
            currentAnim = idleAnim.get();

            initialized = true;
            std::cout << "MultiAnimation initialized: Idle (row 0), Jump (row 1)\n";
        }
        catch (const std::exception& e) {
            std::cout << "ERROR initializing MultiAnimation: " << e.what() << "\n";
            return;
        }
    }

    if (!animTexture || !currentAnim) {
        std::cout << "MultiAnimation not properly initialized!\n";
        return;
    }

    // Handle input to switch animations
    if (Input::IsKeyDown(KEY_SPACE)) {
        currentAnim = jumpAnim.get();
    }
    else {
        currentAnim = idleAnim.get();
    }

    // Use delta time
    float deltaTime = Time::FixedDeltaTime();

    // Position & size for the "main character"
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f };

    // Update and draw current animation
    Sprite currentSprite = currentAnim->play(pos, size, deltaTime);

    shader->use();
    shader->setMat4("uProjection", r2d->GetProjection());
    renderer->beginFrame();
    renderer->submitSprite(currentSprite);
    renderer->endFrame();
}

#if 0
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
#endif

#if 1
void GraphicsTestScene::runBatchStress() {
    auto* r2d = GetWorld().GetSystem<Engine::Renderer2D>();
    if (!r2d) {
        std::cout << "Renderer2D not found!\n";
        return;
    }

    auto* renderer = r2d->GetRenderer();
    auto* shader = r2d->GetShader();

    // Load texture once
    static bool initialized = false;
    static Texture texture("assets/textures/test/fishBoy.png");
    static GLuint textureId = 0;

    struct SpriteData {
        glm::vec2 pos;
        glm::vec2 size;
        glm::vec4 baseColor;   // static hue
        float rotationOffset;  // per-sprite random phase
        float rotationSpeed;   // per-sprite spin speed
    };

    static std::vector<SpriteData> sprites;

    if (!initialized) {
        textureId = texture.ID();
        sprites.reserve(2500);

        for (int i = 0; i < 2500; ++i) {
            float x = static_cast<float>(rand() % static_cast<int>(m_worldWidth));
            float y = static_cast<float>(rand() % static_cast<int>(m_worldHeight));

            // Random base color (RGB only, alpha = 1)
            glm::vec4 col = {
                (rand() % 100) / 100.f,
                (rand() % 100) / 100.f,
                (rand() % 100) / 100.f,
                1.f
            };

            // Random speed between 30–120 degrees/sec
            float speed = 30.f + (rand() % 90);

            sprites.push_back({
                { x, y },                 // position
                { 32.f, 32.f },           // size
                col,                      // base color
                static_cast<float>(rand() % 360), // random phase
                speed                     // per-sprite spin speed
                });
        }

        initialized = true;
        std::cout << "Initialized 2500 rotating quads with random colors\n";
    }

    // Prepare frame
    shader->use();
    shader->setMat4("uProjection", r2d->GetProjection());
    renderer->beginFrame();

    glm::vec4 uv = { 0.f, 0.f, 1.f, 1.f };

    // Use absolute elapsed time for rotation
    double elapsed = Time::ElapsedTime();

    for (const auto& s : sprites) {
        float rotation = s.rotationOffset + s.rotationSpeed * static_cast<float>(elapsed);

        renderer->submitQuad(
            s.pos,
            s.size,
            textureId,
            uv,
            s.baseColor,
            glm::radians(rotation), // convert to radians
            1.f,
            0
        );
    }

    renderer->endFrame();

    // FPS counter
    static float timeAccum = 0.0f;
    static int frameCounter = 0;

    timeAccum += Time::DeltaTime();
    frameCounter++;

    if (timeAccum >= 1.0f) {
        float fps = frameCounter / timeAccum;
        std::cout << "[Batcher test] FPS: " << fps << "\n";

        timeAccum = 0.0f;
        frameCounter = 0;
    }
}
#endif

void GraphicsTestScene::runFontSystem() { /* render text */ }

// ============================================================
// PERFORMANCE / PROFILING DEBUG TESTS
// ============================================================

void GraphicsTestScene::debugPerformance() {
    std::cout << "\n=== PERFORMANCE DEBUGGING ===" << std::endl;
    std::cout << "1) Run with no updates (baseline rendering)" << std::endl;
    std::cout << "2) Try small batch sizes (100, 500, 1000 sprites)" << std::endl;
    std::cout << "3) Verify if all sprites use same texture" << std::endl;
    std::cout << "4) Check buffer capacity vs 2500 sprites (~10k verts, 15k indices)" << std::endl;
    std::cout << "=============================\n" << std::endl;
}

void GraphicsTestScene::testSingleTexture() {
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