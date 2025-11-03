/* Start Header *****************************************************************/
/*!
\file   GraphicsTest.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Implements the GraphicsTestScene, a sandbox scene for testing and validating
rendering features in the engine. This includes rubric-aligned milestone tests
(e.g., basic graphics, sprites, animation, font system) as well as experimental
and performance-focused scenarios.

Features:
- Cycles through test cases interactively with keyboard input (G key).
- Provides tests for basic shapes, sprite rendering, background rendering,
  scaling, rotation, and animation.
- Includes stress tests and profiling utilities for batching performance.
- Validates text rendering with fonts, glyph metrics, kerning, and spacing.
- Outputs FPS and flush counts to the console for performance feedback.
*/
/* End Header *******************************************************************/

// Core
#include "core/Application.h"
#include "core/Logger.h"

// ECS
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"

// Graphics
#include "graphics/font.hpp"
#include "graphics/graphicsConfig.hpp"
#include "graphics/renderer.hpp"
#include "graphics/SpriteMetaData.hpp"

// Helpers
#include "helpers/EntityUtils.h"
#include "helpers/MathUtils.h"

// Services
#include "services/Input.h"
#include "services/ResourceManager.h"
#include "services/Time.h"
#include "services/Window.h"
#include "services/WindowManager.h"

// Test
#include "GraphicsTest.hpp"

// External
#include <glm/glm.hpp>
#include <iostream>

using namespace Sandbox;
using namespace ECS;

constexpr GraphicsTestScene::TestType DEFAULT_TEST  = GraphicsTestScene::TestType::BasicGraphics;
constexpr GraphicsTestScene::TestType LAST_TEST     = GraphicsTestScene::TestType::ObjectPicking;

extern ResourceManager RM;

namespace {
    SpriteMetadata* _getMetaDataForTexture(const std::string& texturePath) {
        // TODO: Maybe shift this somewhere
        auto p = std::filesystem::path(texturePath);
        auto filename = p.stem().string() + ".json";

        auto parent = p.parent_path().parent_path(); // "assets/textures"
        auto metadataPath = parent / "test-metadata" / filename;

        std::ifstream in(metadataPath);
        SpriteMetadata* meta = nullptr;
        if (in) {
            nlohmann::json j;
            in >> j;

            auto key = p.filename().string(); // for example, "fishBoy.png"
            if (j.contains(key)) {
                static std::unordered_map<std::string, SpriteMetadata> cache;
                cache[key] = loadSingleSpriteMetadata(j[key], 0, 0);
                meta = &cache[key];
            }
            else {
                LOG_ERROR("Metadata file " << metadataPath << " missing entry for " << key);
            }
        }
        else {
            LOG_ERROR("Could not open metadata file: " << metadataPath);
        }

        return meta;
    }
}

void GraphicsTestScene::OnLoad() {
    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    CREATE_WINDOW("Graphics Test", windowWidth, windowHeight);
    m_worldWidth = graphicsConfig::PixelsToWorld(static_cast<float>(windowWidth));
    m_worldHeight = graphicsConfig::PixelsToWorld(static_cast<float>(windowHeight));
    m_currentTest = DEFAULT_TEST;

    m_gameplayLayer = GetLayers().CreateOrGetLayer("gameplay");

    m_rendererSystem = std::make_shared<ECS::RendererSystem>();
    m_rendererSystem->Initialize(GetWorld());
    AddSystem([this](Scenes::Scene& s, const float dt){
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    LOG_INFO("GraphicsTestScene initialized");
}

void GraphicsTestScene::OnUpdate() {
    const ECS::World& world = GetWorld();

    // Cycle through test types with G
    if (Input::IsKeyDown(KEY_G)) {
        if (!m_gHandled) {
            int current = static_cast<int>(m_currentTest);
            current++;

            if (current > static_cast<int>(LAST_TEST)) {
                current = static_cast<int>(DEFAULT_TEST);
            }

            // cleanup old test entities
            for (const uint64_t id : m_testEntities) {
                const ECS::Entity e = EntityUtils::Unpack(id);
                if (world.IsAlive(e))
                    DestroyEntity(e);
            }
            m_testEntities.clear();

            m_currentTest = static_cast<TestType>(current);
            LOG_INFO("Switched to test " << current);

            m_gHandled = true; // mark handled until key released
        }
    }
    else {
        m_gHandled = false; // reset when key is released
    }

    // ============================================================
    // M1 RUBRIC TESTS
    // ============================================================
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
    // M2 RUBRIC TESTS
    // ============================================================
    case TestType::GameGUI:           runGameGUI();          break;
    case TestType::FontSys:           runFontSys();          break;
    case TestType::ViewportCamera:    runViewportCamera();   break;
    case TestType::TransformationSys: runTransformationSys(); break;
    case TestType::SpriteAnim:        runSpriteAnim();       break;
    case TestType::MultipleShaders:   runMultipleShaders();  break;
    case TestType::Batching:          runBatching();         break;
    case TestType::EditorCamera:      runEditorCamera();     break;
    case TestType::ObjectPicking:     runObjectPicking();    break;

    // ============================================================
    // PERFORMANCE / PROFILING DEBUG TESTS
    // ============================================================
    case TestType::DebugPerformance:  debugPerformance();   break;
    case TestType::SingleTextureTest: testSingleTexture();  break;
    case TestType::AnalyzeRenderer:   analyzeRenderer();    break;
    case TestType::SmallBatchTest:    testSmallBatch();     break;
    }

    if (m_rendererSystem) {
        static double lastTime = Time::ElapsedTime();
        static int frameCount = 0;

        frameCount++;
        const double now = Time::ElapsedTime();

        if (now - lastTime >= 1.0) {
            const double elapsed = now - lastTime;
            const double fps = frameCount / elapsed;

            LOG_DEBUG("FPS: " << fps
                << " | Flushes: " << m_rendererSystem->GetFlushCount());

            frameCount = 0;
            lastTime = now; // reset baseline
        }
    }
}

void GraphicsTestScene::OnUnload() {
    LOG_INFO("GraphicsTestScene shutting down");
    m_batchSprites.clear();
}

// ------------------------------------
// Stub functions (to be implemented later)
// ------------------------------------
void GraphicsTestScene::runBasicGraphics() {
    if (m_testEntities.empty()) {
        // ------------------------------------------------------------
        // Red filled square (center)
        // ------------------------------------------------------------
        const ECS::Entity square = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{0.f, 0.f, 0.f},    // T
                Quaternion{0, 0, 0, 1},     // R
                Vector3D{1.f, 1.f, 1.f}     // S
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeBox2D{
                Vector2D{0.5f, 0.5f},
                Vector2D{0.f, 0.f},
                Color{3.f, 0.f, 0.5f, 1.f}, // Magenta
                0.f,                       // thickness (ignored for filled)
                true                       // filled
            },
            ECS::Components::Name{ "BasicGraphics_Square" }
        );

        // ------------------------------------------------------------
        // circle (left)
        // ------------------------------------------------------------
        const ECS::Entity filledCircle = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{5.f, 0.f, 0.f},
                Quaternion{0, 0, 0, 1},
                Vector3D{1.f, 1.f, 1.f}
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                0.5f,                          // radius
                Vector2D{0.f, 0.f},            // offset
                Color{2.f, 1.4f, 7.4f, 1.f},
                0.f                            // strokePx (0 = filled)
            },
            ECS::Components::Name{ "Filled_Circle" }
        );

        // ------------------------------------------------------------
        // Outlined green circle (center-right)
        // ------------------------------------------------------------
        const ECS::Entity outlinedCircle = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{0.f, 2.5f, 0.f},
                Quaternion{0, 0, 0, 1},
                Vector3D{1.f, 1.f, 1.f}
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                0.5f,                          // radius
                Vector2D{0.f, 0.f},            // offset
                Color{0.f, 4.f, 0.f, 1.f},     // green outline
                graphicsConfig::PixelsToWorld(50.f)                         // strokePx > 0 = outline
            },
            ECS::Components::Name{ "Outlined_Circle" }
        );

        // ------------------------------------------------------------
        // Hollow magenta collider circle (semi-transparent outline)
        // ------------------------------------------------------------
        const ECS::Entity colliderCircle = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{-5.f, 0.f, 0.f},
                Quaternion{0, 0, 0, 1},
                Vector3D{1.f, 1.f, 1.f}
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                0.6f,                          // radius
                Vector2D{0.f, 0.f},            // offset
                Color{4.5f, 1.2f, 0.2f, 1.0f},
                0.f                           
            },
            ECS::Components::Name{ "Collider_Circle" }
        );

        // Track created entities
        m_testEntities.push_back(EntityUtils::Pack(square));
        m_testEntities.push_back(EntityUtils::Pack(filledCircle));
        m_testEntities.push_back(EntityUtils::Pack(outlinedCircle));
        m_testEntities.push_back(EntityUtils::Pack(colliderCircle));

        LOG_DEBUG("Spawned BasicGraphics test entities");
    }
}

void GraphicsTestScene::runDebugDrawing() {
    if (m_testEntities.empty()) {
        ECS::World& world = GetWorld();
        const std::string spritePath = "assets/textures/test/player.png";

        // Load texture (if valid)
        const auto tex = RM.Get<Texture>(spritePath);
        if (!tex) {
            LOG_ERROR("Failed to load sprite texture: " << spritePath);
            return;
        }

        // Create the sprite entity
        const ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ 0.f, 0.f, 0.f },
                Quaternion{ 0.f, 0.f, 0.f, 1.f },
                Vector3D{
                    graphicsConfig::PixelsToWorld(static_cast<float>(tex->Width())),
                    graphicsConfig::PixelsToWorld(static_cast<float>(tex->Height())),
                    1.f
                }
            },
            ECS::Components::WorldTransform{ },
            ECS::Components::SpriteRenderer2D{
                tex->ID(),
                Color{ 1.f, 1.f, 1.f, 1.f },
                Vector2D{ 1.f, 1.f },
                Vector2D{ 0.f, 0.f }
            },
            ECS::Components::Name{ "DebugDrawing_Sprite" }
        );

        // Store for later cleanup
        m_testEntities.push_back(EntityUtils::Pack(sprite));

        LOG_DEBUG("Spawned DebugDrawing sprite entity");
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
    Expected outcome: Two 256�256 sprites appear centered at 40% and 60% of the
    screen width, showing their PNG textures accurately without color distortion
    at the edges.
*/
//------------------------------------------------------------------------------
void GraphicsTestScene::runBasicSprites() {

    if (!m_testEntities.empty()) return;

    const std::string playerSpritePath = "assets/textures/test/player.png";
    const std::string fishBoySpritePath = "assets/textures/test/fishBoy.png";

    auto playerTexture = RM.Get<Texture>(playerSpritePath);
    uint32_t playerTexId = playerTexture ? playerTexture->ID() : 0;

    auto fishBoyTexture = RM.Get<Texture>(fishBoySpritePath);
    uint32_t fishBoyTexId = fishBoyTexture ? fishBoyTexture->ID() : 0;

    //auto* playerMeta = _getMetaDataForTexture(playerSpritePath);
    //auto* fishBoyMeta = _getMetaDataForTexture(fishBoySpritePath);
    
    ECS::Entity sprite1 = CreateOnLayer(
        m_gameplayLayer,
        ECS::Components::LocalTransform{
            Vector3D{
                -2,
                0,
                0
            },
            Quaternion{0, 0, 0, 1},
            Vector3D{
                4.f,
                4.f,
                1.f
            }
        },
        ECS::Components::WorldTransform{},
        ECS::Components::Name{"PlayerSprite"},
        ECS::Components::SpriteRenderer2D{
            playerTexId,
            Color{1.f,1.f,1.f,1.f},
            Vector2D{1.f,1.f},
            Vector2D{0.f,0.f}
        }
    );

    m_testEntities.push_back(EntityUtils::Pack(sprite1));

    ECS::Entity sprite2 = CreateOnLayer(
        m_gameplayLayer,
        ECS::Components::LocalTransform{
            Vector3D{
                2,
                0,
                0
            },
            Quaternion{0, 0, 0, 1},
            Vector3D{
                4.f,
                4.f,
                1.f
            }
        },
        ECS::Components::WorldTransform{},
        ECS::Components::Name{"FishBoySprite"},
        ECS::Components::SpriteRenderer2D{
            fishBoyTexId,
            Color{1.f,1.f,1.f,1.f},
            Vector2D{1.f,1.f},
            Vector2D{0.f,0.f}
        }
    );

    m_testEntities.push_back(EntityUtils::Pack(sprite2));

    LOG_DEBUG("Spawned BasicSprites entities");
}

void GraphicsTestScene::runBackground() {
    if (!m_testEntities.empty()) return;
    
    ECS::Entity bg = CreateOnLayer(
        m_gameplayLayer,
        ECS::Components::LocalTransform{
            Vector3D{0, 0, 0},
            Quaternion{0,0,0,1},
            Vector3D{m_worldWidth, m_worldHeight, 1.f}
        },
        ECS::Components::WorldTransform{},
        ECS::Components::SpriteRenderer2D{
            RM.Get<Texture>("assets/textures/test/johnPork.png")->ID(),
            Color{1.f,1.f,1.f,1.f},
            Vector2D{1.f,1.f},
            Vector2D{0.f,0.f}
        },
        ECS::Components::Name{"Background_JohnPork"}
    );

    // Store the entity ID so it persists across frames
    m_testEntities.push_back(EntityUtils::Pack(bg));

    LOG_DEBUG("Background loaded: johnPork.png covering " 
              << m_worldWidth << "x" << m_worldHeight);
}

void GraphicsTestScene::runSpriteScaling() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{0, 0, 0},
                Quaternion{0,0,0,1},
                Vector3D{3, 3, 1.f}
            },
            ECS::Components::WorldTransform{ },
            ECS::Components::SpriteRenderer2D{
                RM.Get<Texture>("assets/textures/test/player.png")->ID(),
                Color{1.f,1.f,1.f,1.f},
                Vector2D{1.f,1.f},
                Vector2D{0.f,0.f}
            },
            ECS::Components::Name{ "SpriteScaling_Player" }
        );

        m_testEntities.push_back(EntityUtils::Pack(sprite)); // store ID for later
    }

    // Update every frame
    float scaleSpeed = 200.f;

    ECS::Entity sprite = EntityUtils::Unpack(m_testEntities[0]); // reconstruct from ID
    auto& tr = world.Get<ECS::Components::LocalTransform>(sprite);

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
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{0, 0, 0},
                Quaternion{0,0,0,1},
                Vector3D{4, 4, 1.f}
            },
            ECS::Components::WorldTransform{ },
            ECS::Components::SpriteRenderer2D{
                RM.Get<Texture>("assets/textures/test/fishBoy.png")->ID(),
                Color{1.f,1.f,1.f,1.f},
                Vector2D{1.f,1.f},
                Vector2D{0.f,0.f}
            },
            ECS::Components::Name{ "SpriteRotation_FishBoy" }
        );

        m_testEntities.push_back(EntityUtils::Pack(sprite));
        LOG_DEBUG("Spawned SpriteRotation entity");
    }

    // Update every frame
    float rotationSpeed = 180.f; // degrees per second

    ECS::Entity sprite = EntityUtils::Unpack(m_testEntities[0]); // reconstruct from ID
    auto& tr = world.Get<ECS::Components::LocalTransform>(sprite);

    if (Input::IsKeyDown(KEY_R)) {
        // rotationSpeed is in DEGREES/second; convert to RADIANS/second
        const float angleRad = glm::radians(rotationSpeed) * Time::DeltaTime();

        // rotate about Z for 2D sprites
        const auto deltaRotation = Quaternion::FromAxisAngle(Vector3D{ 0.f, 0.f, 1.f }, angleRad);

        tr.Rotation = deltaRotation * tr.Rotation; // apply incremental rotation
        tr.Rotation.Normalize();
    }
}

void GraphicsTestScene::runAnimation() {
    ECS::World& world = GetWorld();

    if (!m_rendererSystem) return;
    auto* renderer = m_rendererSystem->GetRenderer();
    auto* shader = m_rendererSystem->GetShader();

    // Static variables for animation state
    static bool initialized = false;
    static std::unique_ptr<SpriteAnimation> anim;
    static std::unique_ptr<Texture> animTexture; // Keep texture alive

    // Initialize animation once
    if (!initialized) {
        try {
            animTexture = std::make_unique<Texture>("assets/textures/test/FishfolkSheet.png");
            GLuint texId = animTexture->ID();
            if (texId == 0) { LOG_ERROR("ERROR: Failed to load animation texture!"); return; }

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
            LOG_DEBUG("Animation initialized - Sheet: "
                << animTexture->Width() << "x" << animTexture->Height()
                << ", Frame: " << frameWidth << "x" << frameHeight);
        }
        catch (const std::exception& e) {
            LOG_ERROR("ERROR initializing animation: " << e.what());
            return;
        }
    }

    if (!anim || !animTexture) {
        LOG_ERROR("Animation not properly initialized!");
        return;
    }

    // Use the Time system's delta time directly
    float deltaTime = Time::DeltaTime();

    // Update animation and get current frame
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f }; // Scale up for visibility

    Sprite currentSprite = anim->play(pos, size, deltaTime);

    // Submit to renderer
    shader->use();
    shader->setMat4("uViewProj", m_rendererSystem->GetProjection());
    renderer->beginFrame();
    renderer->submitSprite(currentSprite);
    renderer->endFrame();

    // Debug info (print occasionally)
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 120 == 0) { // Every 2 seconds at 60fps
        LOG_DEBUG("Animation running - Current frame at: "
            << currentSprite.pos.x << ", " << currentSprite.pos.y);
    }
}

void GraphicsTestScene::runMultiAnimation() {
    ECS::World& world = GetWorld();

    if (!m_rendererSystem) return;
    auto* renderer = m_rendererSystem->GetRenderer();
    auto* shader = m_rendererSystem->GetShader();

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
                LOG_ERROR("ERROR: Failed to load animation texture!");
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
            LOG_DEBUG("MultiAnimation initialized: Idle (row 0), Jump (row 1)");
        }
        catch (const std::exception& e) {
            LOG_ERROR("ERROR initializing MultiAnimation: " << e.what());
            return;
        }
    }

    if (!animTexture || !currentAnim) {
        LOG_ERROR("MultiAnimation not properly initialized!");
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
    float deltaTime = Time::DeltaTime();

    // Position & size for the "main character"
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f };

    // Update and draw current animation
    Sprite currentSprite = currentAnim->play(pos, size, deltaTime);

    shader->use();
    shader->setMat4("uViewProj", m_rendererSystem->GetProjection());
    renderer->beginFrame();
    renderer->submitSprite(currentSprite);
    renderer->endFrame();
}

void GraphicsTestScene::runBatchStress() {
    ECS::World& world = GetWorld();

    // One-time initialization
    if (m_testEntities.empty()) {
        // Load texture and verify it worked
        auto fishBoyTexture = RM.Get<Texture>("assets/textures/test/fishBoy.png");
        if (!fishBoyTexture) {
            LOG_ERROR("Failed to load fishBoy.png texture!");
            return;
        }

        const GLuint texId = fishBoyTexture->ID();
        if (texId == 0) {
            LOG_ERROR("Texture loaded but ID is 0!");
            return;
        }

        LOG_DEBUG("Loaded fishBoy.png - Texture ID: " << texId);

        // Sprite size in world units (32px sprite = 0.32 world units)
        constexpr float spriteWU = graphicsConfig::PixelsToWorld(32.0f);

        // Calculate spawn bounds centered at origin
        const float halfWidth = m_worldWidth * 0.5f;
        const float halfHeight = m_worldHeight * 0.5f;

        // Spawn 2500 sprites with random positions and colors
        constexpr int kCount = 2500;
        for (int i = 0; i < kCount; ++i) {
            // Random position in world space (0 to worldWidth/Height)
            const float x = (static_cast<float>(rand() % 100) / 100.0f * m_worldWidth) - halfWidth;
            const float y = (static_cast<float>(rand() % 100) / 100.0f * m_worldHeight) - halfHeight;

            // Random vibrant color (avoid near-black or near-white)
            const Color randomColor{
                0.3f + (rand() % 70) / 100.0f,  // 0.3 to 1.0
                0.3f + (rand() % 70) / 100.0f,
                0.3f + (rand() % 70) / 100.0f,
                1.0f
            };

            // Random rotation speed (30-120 degrees per second)
            const float rotSpeed = 30.0f + static_cast<float>(rand() % 90);

            // Random starting rotation offset (0-360 degrees)
            const float rotOffset = static_cast<float>(rand() % 360);

            // Create entity with all components
            ECS::Entity sprite = CreateOnLayer(
                m_gameplayLayer,
                ECS::Components::LocalTransform{
                    Vector3D{ x, y, 0.0f },
                    Quaternion::FromAxisAngle(Vector3D{ 0.f, 0.f, 1.f }, glm::radians(rotOffset)),
                    Vector3D{ spriteWU, spriteWU, 1.0f }
                },
                ECS::Components::WorldTransform{},
                ECS::Components::SpriteRenderer2D{
                    texId,
                    randomColor,
                    Vector2D{ 1.0f, 1.0f },
                    Vector2D{ 0.0f, 0.0f }
                },
                ECS::Components::Rotator{
                    rotSpeed,
                    rotOffset
                },
                ECS::Components::Name{ "BatchStress_Sprite_" }
            );

            m_testEntities.push_back(EntityUtils::Pack(sprite));
        }

        LOG_DEBUG("Spawned " << kCount << " rotating sprites for batch stress test");
        LOG_DEBUG("  Sprite size: " << spriteWU << " world units (" << graphicsConfig::WorldToPixels(spriteWU) << " pixels)");
        LOG_DEBUG("  Spawn area: " << m_worldWidth << " × " << m_worldHeight << " world units");
    }

    // Per-frame rotation update
    const double elapsed = Time::ElapsedTime();

    world.Each<ECS::Components::LocalTransform, ECS::Components::Rotator>(
        [elapsed](ECS::Entity /*e*/,
            ECS::Components::LocalTransform& lt,
            const ECS::Components::Rotator& rot)
        {
            // Calculate current rotation: offset + speed * time
            const float degrees = rot.RotationOffset + rot.RotationSpeed * static_cast<float>(elapsed);
            const float radians = glm::radians(degrees);

            // Update rotation quaternion
            lt.Rotation = Quaternion::FromAxisAngle(Vector3D{ 0.f, 0.f, 1.f }, radians);
        }
    );

    // FPS counter (print once per second)
    static float timeAccum = 0.0f;
    static int frameCounter = 0;

    timeAccum += Time::DeltaTime();
    frameCounter++;

    if (timeAccum >= 1.0f) {
        const float fps = frameCounter / timeAccum;
        LOG_DEBUG("[BatchStress] FPS: " << fps
            << " | Sprites: " << m_testEntities.size());

        timeAccum = 0.0f;
        frameCounter = 0;
    }
}

void GraphicsTestScene::runFontSystem() {
    if (m_testEntities.empty()) {

        // Design at reference resolution (1920×1080)
        // Positions and font sizes will scale automatically

        // Top-left: Poetry quote (50px offset from corner)
        ECS::Entity text1 = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ 50.f, 80.f, 0.0f },  // Offset from anchor at 1920x1080
                Quaternion::Identity(),
                Vector3D{ 1.0f, 1.0f, 1.0f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::Text{
                "Do not go gentle into that good night,\n"
                "Old age should burn and rave at close of day;",
                61.0f,  // Font size at 1920x1080
                Color{ 1.0f, 1.0f, 1.0f, 0.8f },
                ECS::Components::TextAnchor::TopLeft
            },
            ECS::Components::Name{ "Poetry" }
        );

        // Center: Kerning test
        ECS::Entity text2 = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ 0.f, -100.f, 0.0f },  // 100px below center
                Quaternion::Identity(),
                Vector3D{ 1.0f, 1.0f, 1.0f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::Text{
                "AV AW To Yo Wa Fo\n"
                "if fi fl fj fk yj yp\n"
                "mmm iii lll HHO UI Il Ty Fo\n"
                "i.i i,i i!i i?i T.T T,T T!T T?T\n",
                40.0f,
                Color{ 1.0f, 1.0f, 1.0f, 0.8f },
                ECS::Components::TextAnchor::Center
            },
            ECS::Components::Name{ "Kerning" }
        );

        // Bottom-left: Shakespeare with Open Sans font
        ECS::Entity text3 = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ 100.f, 100.f, 0.0f },
                Quaternion::Identity(),
                Vector3D{ 1.0f, 1.0f, 1.0f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::Text{
                "Men at some time are masters of their fates.\n"
                "The fault, dear Brutus, is not in our stars,\n"
                "but in ourselves, that we are underlings.",
                24.0f,
                Color{ 1.0f, 1.0f, 1.0f, 0.8f },
                ECS::Components::TextAnchor::BottomLeft,
                "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf"  // Font path here
            },
            ECS::Components::Name{ "Shakespeare" }
        );

        // Top-right: FPS counter
        ECS::Entity fps = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ 140.f, 40.f, 0.0f },
                Quaternion::Identity(),
                Vector3D{ 1.0f, 1.0f, 1.0f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::Text{
                "FPS: 0",
                32.0f,
                Color{ 0.0f, 1.0f, 0.0f, 1.0f },
                ECS::Components::TextAnchor::TopRight
            },
            ECS::Components::Name{ "FPS" }
        );

        m_testEntities.push_back(EntityUtils::Pack(text1));
        m_testEntities.push_back(EntityUtils::Pack(text2));
        m_testEntities.push_back(EntityUtils::Pack(text3));
        m_testEntities.push_back(EntityUtils::Pack(fps));

        LOG_DEBUG("Spawned scaled text entities");
    }

    // Update FPS counter
    ECS::World& world = GetWorld();
    ECS::Entity fpsEntity = EntityUtils::Unpack(m_testEntities.back());

    if (world.Has<Components::Text>(fpsEntity)) {
        auto& fpsText = world.Get<Components::Text>(fpsEntity);

        static float updateTimer = 0.0f;
        updateTimer += Time::DeltaTime();

        if (updateTimer >= 0.5f) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "FPS: %.1f", 1.0f / Time::DeltaTime());
            fpsText.setContent(buffer);
            updateTimer = 0.0f;
        }
    }
}

// ============================================================
// M2 TEST RUNNERS
// ============================================================

void Sandbox::GraphicsTestScene::runGameGUI() {
    // TODO: Implement M2 Game GUI test
}

void Sandbox::GraphicsTestScene::runFontSys() {
    runFontSystem();
}

void GraphicsTestScene::runViewportCamera() {
    // ------------------------------------------------------------
    // INITIALIZATION (runs once)
    // ------------------------------------------------------------
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // -------------------------------
        // Player (cyan circle)
        // -------------------------------
        ECS::Entity player = world.Create();

        auto& playerTr = world.Add<ECS::Components::LocalTransform>(player);
        playerTr.Position = { 0.f, 0.f, 0.f };
        playerTr.Scale = { 1.f, 1.f, 1.f };
        playerTr.Rotation = Quaternion::Identity();

        auto& playerCircle = world.Add<ECS::Components::ShapeCircle2D>(player);
        playerCircle.Radius = 0.55f;                    // world units
        playerCircle.Color = { 0.f, 2.f, 2.f, 1.f };    // cyan
        playerCircle.Thickness = 1.0f;                  // used if not Filled
        playerCircle.Filled = true;

        auto& playerLayer = world.Add<ECS::Components::Layer>(player);
        playerLayer.Id = m_gameplayLayer;

        // -------------------------------
        // Camera (follows player)
        // -------------------------------
        ECS::Entity cam = world.Create();

        auto& camTr = world.Add<ECS::Components::LocalTransform>(cam);
        camTr.Position = { playerTr.Position.X, playerTr.Position.Y, 10.f }; // Z = camera height
        camTr.Scale = { 1.f, 1.f, 1.f };
        camTr.Rotation = Quaternion::Identity();

        auto& camera = world.Add<ECS::Components::Camera3D>(cam);
        camera.Active = true;
        camera.UsePerspective = false;
        camera.OrthoSize = 16.f;
        camera.NearPlane = 0.1f;
        camera.FarPlane = 100.f;

        // Aspect from current window
        const auto window = WindowManager::GetMainWindow();
        camera.AspectRatio = static_cast<float>(window->Width())
            / static_cast<float>(window->Height());

        // -------------------------------
        // Static box object
        // -------------------------------
        ECS::Entity box = world.Create();

        auto& boxTr = world.Add<ECS::Components::LocalTransform>(box);
        boxTr.Position = { 0.f, 0.f, 0.f };
        boxTr.Scale = { 2.f, 2.f, 1.f };
        boxTr.Rotation = Quaternion::Identity();

        auto& boxShape = world.Add<ECS::Components::ShapeBox2D>(box);
        boxShape.HalfExtents = { 1.f, 1.f };            // half of 2x2
        boxShape.Color = { 1.f, 0.f, 0.5f, 1.f }; // pink
        boxShape.Thickness = 1.0f;
        boxShape.Filled = true;

        auto& boxLayer = world.Add<ECS::Components::Layer>(box);
        boxLayer.Id = m_gameplayLayer;

        // Track created entities (store ECS::Entity directly)
        m_testEntities.push_back(EntityUtils::Pack(player));
        m_testEntities.push_back(EntityUtils::Pack(cam));
        m_testEntities.push_back(EntityUtils::Pack(box));

        std::cout << "Viewport camera test initialized\n";
    }

    // ------------------------------------------------------------
    // UPDATE LOOP (runs every frame)
    // ------------------------------------------------------------
    ECS::Entity playerEntity = EntityUtils::Unpack(m_testEntities[0]);
    ECS::Entity camEntity = EntityUtils::Unpack(m_testEntities[1]);

    auto& playerTr = world.Get<ECS::Components::LocalTransform>(playerEntity);
    auto& camTr = world.Get<ECS::Components::LocalTransform>(camEntity);
    auto& camComp = world.Get<ECS::Components::Camera3D>(camEntity); // if you need params

    // Movement input (keep your units the same)
    const float dt = Time::DeltaTime();
    const float moveSpeed = graphicsConfig::PixelsToWorld(500.f) * dt;

    if (Input::IsKeyDown(KEY_A)) playerTr.Position.X -= moveSpeed;
    if (Input::IsKeyDown(KEY_D)) playerTr.Position.X += moveSpeed;
    if (Input::IsKeyDown(KEY_W)) playerTr.Position.Y += moveSpeed;
    if (Input::IsKeyDown(KEY_S)) playerTr.Position.Y -= moveSpeed;

    // Smooth camera follow (XY only; keep Z = camera height)
    // Controls how quickly the camera catches up (seconds)
    const float tau = 0.3f;          // increase for more lag
    const float alpha = 1.0f - expf(-dt / tau);

    camTr.Position.X += alpha * (playerTr.Position.X - camTr.Position.X);
    camTr.Position.Y += alpha * (playerTr.Position.Y - camTr.Position.Y);

    // camTr.Position.Z stays as is (e.g., 10.f)
}


void Sandbox::GraphicsTestScene::runTransformationSys() {
    // TODO: Implement M2 Transformation System test
}

void Sandbox::GraphicsTestScene::runSpriteAnim() {
    runAnimation();
}

void GraphicsTestScene::runMultipleShaders() {
    if (m_testEntities.empty()) {
        const float midX = m_worldWidth * 0.5f;
        const float midY = m_worldHeight * 0.5f;

        // ---------------------------------------------
        // Player: controllable yellow circle (filled)
        // ---------------------------------------------
        ECS::Entity player = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ midX, midY, 0.f },
                Quaternion::Identity(),
                Vector3D{ 1.f, 1.f, 1.f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                60.f,
                Vector2D{ 0.f, 0.f },
                Color{ 9.f, 3.f, 0.f, 1.f }, // brighter yellow
                0.f                          // filled
            },
            ECS::Components::Name{ "Player" }
        );

        // ---------------------------------------------
        // Static red circle (left side)
        // ---------------------------------------------
        ECS::Entity redCircle = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ midX - 250.f, midY, 0.f },
                Quaternion::Identity(),
                Vector3D{ 1.f, 1.f, 1.f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                50.f,
                Vector2D{ 0.f, 0.f },
                Color{ 1.f, 0.f, 0.f, 1.f },
                0.f
            },
            ECS::Components::Name{ "RedCircle" }
        );

        // ---------------------------------------------
        // Static green outlined circle (right side)
        // ---------------------------------------------
        ECS::Entity greenOutline = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{
                Vector3D{ midX + 250.f, midY, 0.f },
                Quaternion::Identity(),
                Vector3D{ 1.f, 1.f, 1.f }
            },
            ECS::Components::WorldTransform{},
            ECS::Components::ShapeCircle2D{
                60.f,
                Vector2D{ 0.f, 0.f },
                Color{ 0.f, 1.f, 0.f, 1.f },
                3.f // outline stroke
            },
            ECS::Components::Name{ "GreenOutlinedCircle" }
        );

        // Track entities
        m_testEntities.push_back(EntityUtils::Pack(player));
        m_testEntities.push_back(EntityUtils::Pack(redCircle));
        m_testEntities.push_back(EntityUtils::Pack(greenOutline));

        LOG_DEBUG("Spawned player and test circles for MultipleShaders test");
    }

    // ---------------------------------------------
    // Simple player movement logic
    // ---------------------------------------------
    ECS::World& world = GetWorld();
    ECS::Entity playerEntity = EntityUtils::Unpack(m_testEntities[0]);
    auto& playerTr = world.Get<ECS::Components::LocalTransform>(playerEntity);

    const float moveSpeed = 400.f * Time::DeltaTime();
    if (Input::IsKeyDown(KEY_A)) playerTr.Position.X -= moveSpeed;
    if (Input::IsKeyDown(KEY_D)) playerTr.Position.X += moveSpeed;
    if (Input::IsKeyDown(KEY_W)) playerTr.Position.Y += moveSpeed;
    if (Input::IsKeyDown(KEY_S)) playerTr.Position.Y -= moveSpeed;
}

void Sandbox::GraphicsTestScene::runBatching() {
    // TODO: Implement M2 Batching / draw-call reduction test
}

void Sandbox::GraphicsTestScene::runEditorCamera() {
    // TODO: Implement M2 Editor Camera test
}

void Sandbox::GraphicsTestScene::runObjectPicking() {
    // TODO: Implement M2 Object Picking test
}

// ============================================================
// PERFORMANCE / PROFILING DEBUG TESTS
// ============================================================

void GraphicsTestScene::debugPerformance() {
    std::cout << "\n=== PERFORMANCE DEBUGGING ===" << '\n';
    std::cout << "1) Run with no updates (baseline rendering)" << '\n';
    std::cout << "2) Try small batch sizes (100, 500, 1000 sprites)" << '\n';
    std::cout << "3) Verify if all sprites use same texture" << '\n';
    std::cout << "4) Check buffer capacity vs 2500 sprites (~10k verts, 15k indices)" << '\n';
    std::cout << "=============================\n" << '\n';
}

void GraphicsTestScene::testSingleTexture() {
    if (m_testEntities.empty()) {
        const int count = 2500;
        const float spacingX = m_worldWidth / 50.0f;
        const float spacingY = m_worldHeight / 50.0f;

        for (int i = 0; i < count; ++i) {
            int col = i % 50;
            int row = i / 50;

            ECS::Entity sprite = CreateOnLayer(
                m_gameplayLayer,
                ECS::Components::LocalTransform{
                    Vector3D{
                        col * spacingX + spacingX * 0.5f,
                        row * spacingY + spacingY * 0.5f,
                        0
                    },
                    Quaternion{0,0,0,1},
                    Vector3D{32.f,32.f,1.f}
                },
                ECS::Components::WorldTransform{},
                ECS::Components::SpriteRenderer2D{
                    RM.Get<Texture>("assets/textures/test/fishBoy.png")->ID(),
                    Color{1.f,1.f,1.f,1.f},
                    Vector2D{1.f,1.f},
                    Vector2D{0.f,0.f}
                }
            );

            m_testEntities.push_back(EntityUtils::Pack(sprite));
        }

        LOG_DEBUG("Spawned " << count << " identical sprites (same texture & color)");
        return;
    }

    // No updates => measure raw rendering cost
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        LOG_DEBUG("SingleTexture FPS: " << (1.0f / Time::DeltaTime()));
    }
}

void GraphicsTestScene::analyzeRenderer() {
    ECS::World& world = GetWorld();

    if (!m_rendererSystem) {
        LOG_ERROR("RendererSystem system not found!");
        return;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        int flushes = m_rendererSystem->GetFlushCount();
        LOG_DEBUG("=== RENDERER ANALYSIS ===");
        LOG_DEBUG("Flushes this frame: " << flushes);
        if (flushes > 10) {
            LOG_DEBUG("Too many flushes! Likely texture switches or buffer overflows...");
        }
        else if (flushes == 1) {
            LOG_DEBUG("Single batch, bottleneck is CPU-side or GPU fillrate");
        }
        LOG_DEBUG("FPS: " << (1.0f / Time::DeltaTime()));
        LOG_DEBUG("=========================");
    }
}

void GraphicsTestScene::testSmallBatch() {
    if (m_testEntities.empty()) {
        const int count = 100;
        for (int i = 0; i < count; ++i) {
            ECS::Entity sprite = CreateOnLayer(
                m_gameplayLayer,
                ECS::Components::LocalTransform{
                    Vector3D{
                        100.0f + (i % 10) * 64.0f,
                        100.0f + (i / 10) * 64.0f,
                        0
                    },
                    Quaternion{0,0,0,1},
                    Vector3D{32.f,32.f,1.f}
                },
                ECS::Components::WorldTransform{},
                ECS::Components::SpriteRenderer2D{
                    RM.Get<Texture>("assets/textures/test/fishBoy.png")->ID(),
                    Color{1.f,1.f,1.f,1.f},
                    Vector2D{1.f,1.f},
                    Vector2D{0.f,0.f}
                }
            );

            m_testEntities.push_back(EntityUtils::Pack(sprite));
        }

        LOG_DEBUG("Spawned small batch: " << count << " sprites");
        return;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        float fps = (1.0f / Time::DeltaTime());
        LOG_DEBUG("100-sprite test FPS: " << fps);
        if (fps < 100) {
            LOG_DEBUG("Even 100 sprites are slow, that means the bottleneck is NOT batch size!!!");
        }
    }
}