/* Start Header *****************************************************************/
/*!
\file   GraphicsTest.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Defines the GraphicsTestScene, a sandbox testbed for verifying rendering
features and performance in the engine. It includes rubric-aligned tests
(e.g., basic graphics, sprites, animation, font system) as well as
experimental and performance-focused scenarios such as post-processing,
FBOs, particle systems, and renderer stress tests.

The scene provides a structured way to validate correctness against grading
requirements while also experimenting with advanced rendering features.
*/
/* End Header *******************************************************************/


#pragma once

#include "Game.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "scene/TestScene.h"
#include "ecs/systems/UISystem.h"
#include <vector>
#include <memory>
#include "ecs/systems/RendererSystem.h"

namespace Sandbox {
    class GraphicsTestScene : public Scenes::TestScene {
    public:
        void OnLoad() override;
        void OnUpdate() override;
        void OnUnload() override;

        // NOTE: The enum values (1201 => 1210) are aligned with the official
        // rubric test IDs from M1. This makes it easy to cross-reference
        // between engine code and grading requirements.
        enum class TestType : int {
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

            // Required M2 rubric tests (re-indexed for milestone continuity)
            FontSys             = 1212,    // was 1107
            ViewportCamera      = 1213,    // was 1108
            MultipleShaders     = 1214,    // was 1115
            Batching            = 1215,    // was 1121
            ObjectPicking       = 1216,    // was 1210

            // Extra experimental tests (outside rubric)
            LightingTest        = 2004,
            NormalMaps          = 2005,
            ParticleSystem      = 2006,

            // PERFORMANCE / PROFILING DEBUG TESTS
            DebugPerformance    = 3001,
            SingleTextureTest   = 3002,
            AnalyzeRenderer     = 3003,
            SmallBatchTest      = 3004
        };

    private:
        bool m_gHandled = false;
        uint16_t m_gameplayLayer = 0;
        std::shared_ptr<ECS::RendererSystem> m_rendererSystem;
        std::shared_ptr<ECS::UISystem> m_uiSystem;

        // Background, sprites, debugObjects
        std::vector<PackedEntityId> m_testEntities;

        // Stress test
        std::vector<ECS::Entity> m_batchSprites;

        // World dimensions
        float m_worldWidth = 1600.f;
        float m_worldHeight = 900.f;

        TestType m_currentTest{ TestType::BasicGraphics };

        // ------------------------------------
        // Private test runners (M1 rubric tests)
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

        // ------------------------------------
        // Private test runners (M2 rubric tests)
        // ------------------------------------
        void runFontSys();
        void runViewportCamera();
        void runMultipleShaders();
        void runBatching();
        void runObjectPicking();

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

        void CreateUIButtons(ECS::World& world, uint16_t uiLayer);
    };
}