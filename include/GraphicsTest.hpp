#pragma once

#include "Game.h"
#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"

namespace Sandbox {
    class GraphicsTestScene : public Scene {
    public:
        GraphicsTestScene(int width, int height);

        void OnLoad() override;
        void OnUpdate() override;
        void OnUnload() override;

        // NOTE: The enum values (1201�1210) are aligned with the official
        // rubric test IDs from M1. This makes it easy to cross-reference
        // between engine code and grading requirements.
        enum class TestType {
            // Required M1 rubric tests
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

            // Extra experimental tests (outside rubric)
            SplineDeformation   = 2001,
            FrameBufferObject   = 2002,
            PostProcessing      = 2003,
            LightingTest        = 2004,
            NormalMaps          = 2005,
            ParticleSystem      = 2006,
            SpriteAtlas         = 2007,
            ShaderPlayground    = 2008,

            // PERFORMANCE / PROFILING DEBUG TESTS
            DebugPerformance = 3001,
            SingleTextureTest = 3002,
            AnalyzeRenderer = 3003,
            SmallBatchTest = 3004
        };

    private:
        bool m_gHandled = false;

        // Background, sprites, debugObjects
        std::vector<EntityId> m_TestEntities;

        // Stress test
        std::vector<Entity> m_batchSprites;

        // World dimensions
        float m_worldWidth = 1600.f;
        float m_worldHeight = 900.f;

        TestType m_currentTest{ TestType::BasicGraphics };

        // ------------------------------------
        // Private test runners (declared here)
        // ------------------------------------
        void runBasicGraphics();
        void runDebugDrawing();
        void runBasicSprites();
        void runBackground();
        void runSpriteScaling();
        void runSpriteRotation();
        void runAnimation();
        void runMultiAnimation();
        void runBatchStress();
        void runFontSystem();

        // ===================================================
        // PERFORMANCE PROFILING + DEBUGGING 
        // ===================================================

        //! Run one-off profiling inside stress test
        void debugPerformance();

        //! Spawn all sprites with SAME texture and color
        void testSingleTexture();

        //! Inspect renderer flush behavior and give warnings
        void analyzeRenderer();

        //! Test small batch baseline (100 sprites only)
        void testSmallBatch();
    };
}