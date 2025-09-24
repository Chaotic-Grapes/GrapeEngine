#ifndef PHYSICSCOLLISION2DTEST_H
#define PHYSICSCOLLISION2DTEST_H
#include "Game.h"
#if _DEBUG

#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "DyamicCollision.h"

namespace Sandbox {
    class PhysicsCollision2DTestScene : public Game {
    public:
        PhysicsCollision2DTestScene(int width, int height, float dampingDelay);
        void OnStart(World& world) override;
        void OnUpdate(World& world) override;
        void OnShutdown(World& world) override;
    private:
        void SpawnBalls(World& world, int count, unsigned seed = 0);
        void CreateBoundaryLines(World& world);
        void UpdateBallCollisions();

        std::vector<Entity> m_balls;
        //std::vector<Entity> m_boundaryLines;
        std::vector<Entity> m_seacubes;

        //triangle hero
        void CreateTriangle(World& world);
        void ClampAndBouncePlayer(World& world);

        EntityId m_playerId = UINT32_MAX;
        float m_triHalfHeight = 48.0f;
        float m_triHalfBase = 32.0f;

        //sea cubes
        void SpawnCubes(World& world);
        void SpawnCubes_T(World& world, float dt);
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