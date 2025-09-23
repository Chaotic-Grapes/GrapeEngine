#ifndef PHYSICSCOLLISION2DTEST_H
#define PHYSICSCOLLISION2DTEST_H
#include "Game.h"
#if _DEBUG

#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"

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
        std::vector<Entity> m_boundaryLines;

        //triangle hero
        void CreateTriangle(World& world);
        void UpdateTriangleControls(float dt);
        void ClampAndBouncePlayer(World& world);

        EntityId m_playerId = UINT32_MAX;
        float m_triHalfHeight = 48.0f;
        float m_triHalfBase = 32.0f;

        float m_elapsedTime = 0.0f;
        float m_worldWidth = 1600.f, m_worldHeight = 900.f;
        float m_dampingDelay = 7.f;
        bool m_dampingEnabled = false;

        
    };
}

#endif
#endif