// #ifndef PHYSICSCOLLISION2DTEST_H
// #define PHYSICSCOLLISION2DTEST_H

// #include "Game.h"
// #include "ecs/Entity.h"
// #include "ecs/World.h"
// #include "physics/Collision.h"
// #include "physics/DynamicCollision.h"
// #include "scene/Scene.h"
// #include <unordered_map>
// #include <vector>

// namespace Sandbox {
//     using namespace ECS;

//     class PhysicsCollision2DTestScene : public Scenes::Scene {
//     public:
//         void OnLoad() override;
//         void OnUpdate() override;
//         void OnFixedUpdate() override;
//         void OnUnload() override;

//         enum class TestType {
//             test_PhysicsMovement = 1301,
//             test_CollisionDetection,
//             test_PhysicsHero,
//             test_DynvStatResponse,
//             test_DynvDynResponse,
//             test_StepbyStepUpdate
//         };
        

//     private:
//         bool test_handler = false;
//         bool m_stepInit = false;
//         uint16_t m_gameplayLayer = 0;

//         TestType m_currentTest{ TestType::test_PhysicsMovement };
//         // test types 
//         void Test_PhysicsMovement();
//         void Test_CollisionDetection();
//         void Test_PhysicsHero();
//         void Test_DynVStatResponse();
//         void Test_DynVDynResponse();
//         void Test_StepByStepUpdate();

//         void _clearEntities();       
//         void _resetFlagsAndVariables(); 

//         void _spawnBalls(World& world, int count, unsigned seed = 0);
//         //void CreateBoundaryLines();
//         void _updateBallCollisions();

//         // step-by-step physics debugging
//         bool m_stepByStepMode;
//         bool m_stepRequested;
//         bool m_pausePhysics;

//         void _handleStepByStepControls();
//         void _storeBallStates();
//         void _restoreBallStates();
//         void _ballCollide();
//         struct BallState {
//             Vector2D velocity;
//             Vector2D position;
//         };

//         std::unordered_map<uint32_t, BallState> m_storedBallStates;

//         //storage for entities
//         std::vector<Entity> m_balls;
//         std::vector<Entity> m_seacubes;
//         std::vector<uint64_t> m_staticsquares;

//         //triangle hero
//         void _createTriangle();
//         void _clampAndBouncePlayer();

//         //circle tester
//         void _createHeroCircle();
//         void _clampAndBounceCircleHero();


//         float m_radius = 70.0f;

//         uint64_t m_midLineId = UINT64_MAX;
//         uint64_t m_playerId = UINT64_MAX;
//         float m_triHalfHeight = 48.0f;
//         float m_triHalfBase = 48.0f;

//         float m_restitution = 0.5f;
//         float m_friction = 0.0f;
//         //sea cubes
//         void _spawnCubes();
//         void _spawnCubes_T(float dt);
//         void _updateCubesCollisions(World& world);
//         void _cubeDisintegrate(World& world, size_t i);

//         float m_spawnIntervals = 1.0f;
//         float m_spawnAcc = 0.0f;
//         int   m_maxLeaves = 150;

//         float m_elapsedTime = 0.0f;
//         float m_worldWidth = 1600.f, m_worldHeight = 900.f;
//         float m_dampingDelay = 7.f;
//         bool  m_dampingEnabled = false;
//     };
// }

// #endif