//#ifndef PHYSICSCOLLISION2DTEST_H
//#define PHYSICSCOLLISION2DTEST_H
//
//#include "Game.h"
//#include "ecs/Entity.h"
//#include "ecs/World.h"
//#include "physics/Collision.h"
//#include "physics/DynamicCollision.h"
//#include "scene/Scene.h"
//#include <unordered_map>
//#include <vector>
//
//namespace Sandbox {
//    using namespace ECS;
//
//    class PhysicsCollision2DTestScene final : public Scenes::Scene {
//    public:
//        void OnLoad() override;
//        void OnUpdate() override;
//        void OnFixedUpdate() override;
//        void OnUnload() override;
//
//        enum class TestType : uint16_t {
//            PhysicsMovement = 1301,
//            CollisionDetection,
//            PhysicsHero,
//            DynamicVsStaticResponse,
//            DynamicVsDynamicResponse,
//            StepByStepUpdate
//        };
//
//    private:
//        // Test management
//        bool m_testChangeRequested = false;
//        bool m_testInitialized = false;
//        TestType m_currentTest{ TestType::PhysicsMovement };
//
//        // Test implementations
//        void _testPhysicsMovement();
//        void _testCollisionDetection();
//        void _testPhysicsHero();
//        void _testDynamicVsStaticResponse();
//        void _testDynamicVsDynamicResponse();
//        void _testStepByStepUpdate();
//
//        // Entity spawning and creation
//        void _createHero(bool isCircle);
//        void _createHeroCircle();
//        void _createHeroTriangle();
//        void _createStaticSquare(const char* name, float centerX, float centerY);
//        void _ensureStaticSquares();
//        void _spawnBalls(World& world, int count, unsigned seed = 0);
//        void _spawnCube();
//        void _spawnCubesBatch(float deltaTime);
//
//        // Collision and physics updates
//        void _updateBallBoundaryCollisions();
//        void _updateBallToBallCollisions();
//        void _updateCubeCollisions(World& world);
//        void _updateHeroMovement(float heroRadius);
//        void _applyBoundaryConstraints(Vector3D& position, Vector2D& velocity, float radius);
//        bool _resolveCircleAABBCollision(Vector3D& circlePos, Vector2D& circleVel,
//            const Vector3D& boxMin, const Vector3D& boxMax,
//            float circleRadius);
//
//        // Step-by-step debugging
//        bool m_stepByStepMode = false;
//        bool m_stepRequested = false;
//        bool m_pausePhysics = false;
//
//        void _clearEntities();
//        void _resetFlagsAndVariables();
//
//        struct BallState {
//            Vector2D Velocity;
//            Vector2D Position;
//        };
//        std::unordered_map<uint64_t, BallState> m_storedBallStates;
//
//        void _handleStepByStepControls();
//        void _storeBallStates();
//        void _restoreBallStates();
//
//        // Utility functions
//        void _destroyAllEntities(World& world);
//        void _destroyCube(World& world, size_t index);
//        void _removeCubeAtIndex(size_t index);
//        float _calculateCubeHalfSize(const ShapeRenderer2D* shapeRenderer) const;
//        float _getScaledRadius(float baseRadius, const Vector2D& scale) const;
//        int _buildStaticSquareAABBs(World& world, DynCol::AABB* targets, int maxCount) const;
//        bool _checkSweptAABBCollision(const Vector2D& startPos, const Vector2D& endPos,
//            float halfSize, const DynCol::AABB* targets,
//            int targetCount, float deltaTime) const;
//        bool _shouldDestroyCube(float cubeY, float cubeHalfSize) const;
//
//        // Entity storage
//        std::vector<uint64_t> m_balls;
//        std::vector<uint64_t> m_cubes;
//        std::vector<uint64_t> m_staticSquares;
//
//        uint64_t m_heroId = UINT64_MAX;
//        uint64_t m_midLineId = UINT64_MAX;
//
//        uint16_t m_gameplayLayer = 0;
//
//        // Physics constants
//        static constexpr float HERO_CIRCLE_RADIUS = 70.0f;
//        static constexpr float HERO_TRIANGLE_HALF_HEIGHT = 48.0f;
//        static constexpr float HERO_TRIANGLE_HALF_BASE = 48.0f;
//        static constexpr float HERO_THRUST = 1.0f;
//        static constexpr float HERO_JUMP_IMPULSE = 20.0f;
//        static constexpr float HERO_VELOCITY_DAMPING = 0.996f;
//        static constexpr float STATIC_SQUARE_SIDE_LENGTH = 200.0f;
//        static constexpr float BALL_DAMPING_VALUE = 0.5f;
//        static constexpr float CUBE_SPAWN_INTERVAL = 1.0f;
//        static constexpr int MAX_CUBES = 150;
//        static constexpr float DEFAULT_CUBE_HALF_SIZE = 10.0f;
//
//        float m_restitution = 0.5f;
//        float m_friction = 0.0f;
//
//        // Cube spawning state
//        float m_cubeSpawnAccumulator = 0.0f;
//
//        // World dimensions and timing
//        float m_elapsedTime = 0.0f;
//        float m_worldWidth = 1600.0f;
//        float m_worldHeight = 900.0f;
//        float m_dampingDelay = 7.0f;
//        bool m_dampingEnabled = false;
//    };
//}
//
//#endif
