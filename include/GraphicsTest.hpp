#ifndef GRAPHICSTEST_H
#define GRAPHICSTEST_H
#include "Game.h"
#if _DEBUG

#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"

namespace Sandbox {
    class GraphicsTestScene : public Game {
    public:
        GraphicsTestScene(int width, int height);

        void OnStart(World& world) override;
        void OnUpdate(World& world) override;
        void OnShutdown(World& world) override;

    private:
        enum class TestType {
            BasicGraphics       = 1201,
            DebugDrawing        = 1202,
            BasicSprites        = 1203,
            BackgroundRender    = 1204,
            SpriteScaling       = 1205,
            SpriteRotation      = 1206,
            SpriteAnimation     = 1207,
            MultiAnimation      = 1208,
            PerformanceTest     = 1209,
            FontSystem          = 1210,
        };

        // Background, sprites
        EntityId m_backgroundId = UINT32_MAX;
        EntityId m_sprite1Id = UINT32_MAX;

        // Stress test
        std::vector<Entity> m_batchSprites;

        // World dimensions
        float m_worldWidth = 1600.f;
        float m_worldHeight = 900.f;

        TestType m_currentTest{ TestType::BasicGraphics };

        // ------------------------------------
        // Private test runners (declared here)
        // ------------------------------------
        void runBasicGraphics(World& world);
        void runDebugDrawing(World& world);
        void runBasicSprites(World& world);
        void runBackground(World& world);
        void runSpriteScaling(World& world);
        void runSpriteRotation(World& world);
        void runAnimation(World& world);
        void runMultiAnimation(World& world);
        void runBatchStress(World& world);
        void runFontSystem(World& world);
    };
}

#endif
#endif