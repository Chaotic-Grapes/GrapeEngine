#ifndef PHYSICSCOLLISION2DTEST_H
#define PHYSICSCOLLISION2DTEST_H
#if _DEBUG

#include "Game.h"
#include <vector>
#include <unordered_map>
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "DynamicCollision.h"

namespace Sandbox {
    class PhysicsCollision2DTestScene : public Scene {
    public:
        PhysicsCollision2DTestScene(int width, int height, float dampingDelay);
        void OnLoad() override;
        void OnUpdate() override;
        void OnFixedUpdate() override;
        void OnUnload() override;

        enum class TestType {
            test_PhysicsMovement = 1301,
            test_CollisionDetection,
            test_PhysicsHero,
            test_DynvStatResponse,
            test_DynvDynResponse,
            test_StepbyStepUpdate
        };
        

    private:
        bool test_handler = false;
        bool m_stepInit = false;
        TestType m_currentTest{ TestType::test_PhysicsMovement };
        // test types 
         void Test_PhysicsMovement();
         void Test_CollisionDetection();
         void Test_PhysicsHero();
         void Test_DynVStatResponse();
         void Test_DynVDynResponse();
         void Test_StepByStepUpdate();
        

        void SpawnBalls(World& world, int count, unsigned seed = 0);
        //void CreateBoundaryLines();
        void UpdateBallCollisions();

        // step-by-step physics debugging
        bool m_stepByStepMode;
        bool m_stepRequested;
        bool m_pausePhysics;

        void HandleStepByStepControls();
        void StoreBallStates();
        void RestoreBallStates();
        void BallCollide();
        struct BallState {
            Vector2D velocity;
            Vector2D position;
        };

        std::unordered_map<uint32_t, BallState> m_storedBallStates;

        std::vector<Entity> m_balls;
        //std::vector<Entity> m_boundaryLines;
        std::vector<Entity> m_seacubes;
        std::vector<uint32_t>m_staticsquares;

        //triangle hero
        void CreateTriangle();
        void ClampAndBouncePlayer();

        //circle tester
        void CreateHeroCircle();
        void ClampAndBounceCircleHero();

        float Cradius = 70.0f;


        EntityId m_playerId = UINT32_MAX;
        float m_triHalfHeight = 48.0f;
        float m_triHalfBase = 48.0f;

        float restitution = 0.5f;
        float friction = 0.0f;
        //sea cubes
        void SpawnCubes();
        void SpawnCubes_T(float dt);
        void UpdateCubesCollisions(World& world);
        void CubeDisintegrate(World& world, size_t i);

        float m_spawnIntervals = 1.0f;
        float m_spawnAcc = 0.0f;
        int   m_maxLeaves = 150;

        float m_elapsedTime = 0.0f;
        float m_worldWidth = 1600.f, m_worldHeight = 900.f;
        float m_dampingDelay = 7.f;
        bool m_dampingEnabled = false;
    };
}

#endif
#endif