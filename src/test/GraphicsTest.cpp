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

#include "GraphicsTest.hpp"
#include "core/Application.h"
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/font.hpp"
#include "graphics/renderer.hpp"
#include "helpers/EntityUtils.h"
#include "helpers/MathUtils.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include <glm/glm.hpp>
#include <iostream>
#include "services/ResourceManager.h"


using namespace Sandbox;
using namespace ECS;

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
    m_worldWidth  = static_cast<float>(windowWidth);
    m_worldHeight = static_cast<float>(windowHeight);
    m_currentTest = TestType::BasicGraphics;

    m_gameplayLayer = GetLayers().CreateOrGetLayer("gameplay");

    m_rendererSystem = std::make_shared<ECS::RendererSystem>();
    m_rendererSystem->Initialize();
    AddSystem([this](Scenes::Scene& s, float dt){
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    LOG_INFO("GraphicsTestScene initialized");
}

void GraphicsTestScene::OnUpdate() {
    ECS::World& world = GetWorld();

    // Cycle through test types with G
    if (Input::IsKeyDown(KEY_G)) {
        if (!m_gHandled) {
            int current = static_cast<int>(m_currentTest);
            current++;

            if (current > static_cast<int>(TestType::FontSystem)) {
                current = static_cast<int>(TestType::BasicGraphics);
            }

            // cleanup old test entities
            for (uint64_t id : m_testEntities) {
                ECS::Entity e = EntityUtils::Unpack(id);
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

    if (m_rendererSystem) {
        static double lastTime = Time::ElapsedTime();
        static int frameCount = 0;

        frameCount++;
        double now = Time::ElapsedTime();

        if (now - lastTime >= 1.0) {
            double elapsed = now - lastTime;
            double fps = frameCount / elapsed;

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
        ECS::Entity square = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f,m_worldHeight * 0.5f,0}, Quaternion{0,0,0,1}, Vector3D{100.f,100.f,1} },
            ECS::Components::WorldTransform{ },
            ECS::Components::ShapeBox2D{
                Vector2D{50.f, 50.f}, // half extents
                Vector2D{0.f, 0.f},   // offset
                Color{1.f, 0.f, 0.f, 1.f}, // color
                0.f,                  // thickness
                true                  // filled
            },
            ECS::Components::Name{ "BasicGraphics_Square" }
        );

        m_testEntities.push_back(EntityUtils::Pack(square));
        LOG_DEBUG("Spawned BasicGraphics entity");
    }
}

void GraphicsTestScene::runDebugDrawing() {
    if (m_testEntities.empty()) {
        ECS::World& world = GetWorld();
        const std::string spritePath = "assets/textures/test/player.png";

        auto tex = RM.Get<Texture>(spritePath);
        uint32_t texId = tex ? tex->ID() : 0;

        ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f,m_worldHeight * 0.5f,0}, Quaternion{0,0,0,1}, Vector3D{128.f,128.f,1} },
            ECS::Components::WorldTransform{ },
            ECS::Components::SpriteRenderer2D{
                tex ? tex->ID() : 0,
                Color{1.f,1.f,1.f,1.f},
                Vector2D{1.f,1.f},
                Vector2D{0.f,0.f}
            },
            ECS::Components::Name{ "DebugDrawing_Sprite" }
        );

        auto tr = world.Get<ECS::Components::LocalTransform>(sprite);
        tr.Scale = Vector3D {
            static_cast<float>(tex->Width()),
            static_cast<float>(tex->Height()),
            1.f
        };

        m_testEntities.push_back(EntityUtils::Pack(sprite));

        auto* meta = _getMetaDataForTexture(spritePath);

        ECS::Entity debug = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{ 
                Vector3D{
                    tr.Position.X + meta->collider.offset.x - tr.Scale.X * 0.5f + meta->collider.size.x * 0.5f,
                    tr.Position.Y + meta->collider.offset.y - tr.Scale.Y * 0.5f + meta->collider.size.y * 0.5f,
                    0
                },
                Quaternion{0,0,0,1},
                Vector3D{
                    static_cast<float>(meta->collider.size.x),
                    static_cast<float>(meta->collider.size.y),
                    1.f
                }
            },
            ECS::Components::WorldTransform{ },
            ECS::Components::ShapeBox2D{
                Vector2D{ static_cast<float>(meta->collider.size.x) * 0.5f, static_cast<float>(meta->collider.size.y) * 0.5f }, // half extents
                Vector2D{0.f, 0.f},   // offset
                Color{0.f, 1.f, 0.f, 1.f}, // color
                2.f,                  // thickness
                false                 // filled
            },
            ECS::Components::Name{ "DebugDrawing_SpriteCollider" }
        );

        world.Attach(debug, sprite);
        m_testEntities.push_back(EntityUtils::Pack(debug));

        LOG_DEBUG("Spawned DebugDrawing entities");
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

    auto* playerMeta = _getMetaDataForTexture(playerSpritePath);
    auto* fishBoyMeta = _getMetaDataForTexture(fishBoySpritePath);
    
    ECS::Entity sprite1 = CreateOnLayer(
        m_gameplayLayer,
        ECS::Components::LocalTransform{
            Vector3D{
                m_worldWidth * 0.4f,
                m_worldHeight * 0.5f,
                0
            },
            Quaternion{0, 0, 0, 1},
            Vector3D{
                256.f,
                256.f,
                1.f
            }
        },
        ECS::Components::WorldTransform{},
        ECS::Components::ShapeBox2D{
            Vector2D{128.f, 128.f}, // half extents
            Vector2D{0.f, 0.f},     // offset
            Color{1.f, 1.f, 1.f, 1.f}, // color
            2.f,                    // thickness
            false                   // filled
        },
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
                m_worldWidth * 0.6f,
                m_worldHeight * 0.5f,
                0
            },
            Quaternion{0, 0, 0, 1},
            Vector3D{
                256.f,
                256.f,
                1.f
            }
        },
        ECS::Components::WorldTransform{},
        ECS::Components::ShapeBox2D{
            Vector2D{128.f, 128.f}, // half extents
            Vector2D{0.f, 0.f},     // offset
            Color{1.f, 1.f, 1.f, 1.f}, // color
            2.f,                    // thickness
            false                   // filled
        },
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
            Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0},
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
    World& world = GetWorld();

    if (m_testEntities.empty()) {
        ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f,m_worldHeight * 0.5f,0}, Quaternion{0,0,0,1}, Vector3D{256.f,256.f,1.f} },
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
    auto tr = world.Get<ECS::Components::LocalTransform>(sprite);

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

    if (m_testEntities.empty()) {
        ECS::Entity sprite = CreateOnLayer(
            m_gameplayLayer,
            ECS::Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f,m_worldHeight * 0.5f,0}, Quaternion{0,0,0,1}, Vector3D{512.f,512.f,1.f} },
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
        auto deltaRotation = Quaternion::FromAxisAngle(Vector3D::Right, rotationSpeed * Time::FixedDeltaTime());
        tr.Rotation = deltaRotation * tr.Rotation;
        tr.Rotation.Normalize();
    }
}

void GraphicsTestScene::runAnimation() {
    World& world = GetWorld();

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
    float deltaTime = Time::FixedDeltaTime();

    // Update animation and get current frame
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f }; // Scale up for visibility

    Sprite currentSprite = anim->play(pos, size, deltaTime);

    // Submit to renderer
    shader->use();
    shader->setMat4("uProjection", m_rendererSystem->GetProjection());
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
    World& world = GetWorld();

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
    float deltaTime = Time::FixedDeltaTime();

    // Position & size for the "main character"
    glm::vec2 pos = { m_worldWidth * 0.5f, m_worldHeight * 0.5f };
    glm::vec2 size = { 512.f, 512.f };

    // Update and draw current animation
    Sprite currentSprite = currentAnim->play(pos, size, deltaTime);

    shader->use();
    shader->setMat4("uProjection", m_rendererSystem->GetProjection());
    renderer->beginFrame();
    renderer->submitSprite(currentSprite);
    renderer->endFrame();
}

#if 0
void GraphicsTestScene::runBatchStress() {
    World& world = GetWorld();

    if (m_testEntities.empty()) {
        const int count = 2500; // can increase if performance allows

        for (int i = 0; i < count; ++i) {
            ECS::Entity sprite = CreateOnLayer(
                m_gameplayLayer,
                ECS::Components::LocalTransform{ 
                    Vector3D{
                        MathUtils::Randomize(0, static_cast<int>(m_worldWidth)),
                        MathUtils::Randomize(0, static_cast<int>(m_worldHeight)),
                        0
                    }, 
                    Quaternion::FromAxisAngle(
                        Vector3D::Right,
                        MathUtils::Randomize(0, 360)
                    ),
                    Vector3D{
                        MathUtils::Randomize(16.f, 32.f),
                        MathUtils::Randomize(16.f, 32.f),
                        1.f
                    }
                },
                ECS::Components::WorldTransform{ },
                ECS::Components::SpriteRenderer2D{
                    RM.Get<Texture>("assets/textures/test/fishBoy.png")->ID(),
                    Color{
                        MathUtils::Randomize(0.f, 1.f),
                        MathUtils::Randomize(0.f, 1.f),
                        MathUtils::Randomize(0.f, 1.f),
                        1.f
                    },
                    Vector2D{1.f,1.f},
                    Vector2D{0.f,0.f}
                }
            );

            m_testEntities.push_back(EntityUtils::Pack(sprite));
        }

        std::cout << "Spawned " << m_testEntities.size()
            << " sprites for Batch Stress Test\n";
    }

    // ------------------------------------
    // Per-frame updates
    // ------------------------------------
    for (EntityId id : m_testEntities) {
        ECS::Entity e = EntityUtils::Unpack(id);
        if (!world.IsAlive(e) || !world.Has<ECS::Components::LocalTransform>(e)) continue;
        auto& tr = world.Get<ECS::Components::LocalTransform>(e);

        auto deltaRotation = Quaternion::FromAxisAngle(
            Vector3D::Right,
            90.0f * Time::DeltaTime() // where 90.0f is degrees per second
        );
        tr.Rotation = deltaRotation * tr.Rotation;
        tr.Rotation.Normalize();
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
        LOG_DEBUG("FPS: " << fps);

        timeAccum = 0.0f;
        frameCounter = 0;
    }
}
#endif

#if 1
    void GraphicsTestScene::runBatchStress() {
    if (!m_rendererSystem) {
        LOG_ERROR("RendererSystem not found!");
        return;
    }

    auto* renderer = m_rendererSystem->GetRenderer();
    auto* shader = m_rendererSystem->GetShader();

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

            // Random speed between 30�120 degrees/sec
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
    shader->setMat4("uProjection", m_rendererSystem->GetProjection());
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

void GraphicsTestScene::runFontSystem() {
    if (!m_rendererSystem) return;
    auto* renderer = m_rendererSystem->GetRenderer();
    auto* shader = m_rendererSystem->GetTextShader(); // returns m_shaderText
    if (!shader) return;

    static bool initialized = false;
    static std::unique_ptr<Font> font;

    if (!initialized) {
        font = std::make_unique<Font>("assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf", 96);
        initialized = true;
    }

    shader->use();
    shader->setMat4("uProjection", m_rendererSystem->GetProjection());

    renderer->beginFrame();
    renderer->submitText(*font, "Do not go gentle into that good night,\n Old age should burn and rave at close of day; ", 
                        { 50.f, 200.f }, { 1.f, 1.f, 1.f, 1.f }, 61.f);

    // Test string covers:
    // - Kerning-sensitive pairs (AV, To, Yo, Wa, Fo)
    // - Ascender/descender overlaps (if, fl, yj, yp)
    // - Baseline/bearing alignment (mmm, iii, lll, HHO)
    // - Mixed-case spacing (UI, Il, Ty, Fo)
    // - Punctuation placement (.,?!;:���� -- �)
    // Render this to visually inspect spacing, kerning, and glyph metrics accuracy
    renderer->submitText(*font,
        "AV AW To Yo Wa Fo\n"
        "if fi fl fj fk yj yp\n"
        "mmm iii lll HHO UI Il Ty Fo\n"
        "i.i i,i i!i i?i T.T T,T T!T T?T\n",
        { 400.f, 500.f }, { 1.f, 1.f, 1.f, 0.8f }, 40.f);

    renderer->submitText(*font, "Men at some time are masters of their fates.\n The fault, dear Brutus, is not in our stars, but in ourselves, that we are underlings.",
                        { 100.f, 800.f }, { 1.f, 1.f, 1.f, 0.8f }, 24.f);

    renderer->endFrame();
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

    // No updates � measure raw rendering cost
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        LOG_DEBUG("SingleTexture FPS: " << (1.0f / Time::DeltaTime()));
    }
}

void GraphicsTestScene::analyzeRenderer() {
    World& world = GetWorld();

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