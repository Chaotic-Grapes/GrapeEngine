#ifndef PHYSICSCOLLISION2DTESTSCENE_H
#define PHYSICSCOLLISION2DTESTSCENE_H
#if _DEBUG

#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"

namespace Sandbox {
    class PhysicsCollision2DTestScene : public Engine::ISystem {
    public:
        PhysicsCollision2DTestScene(World* world, float width, float height, float dampingDelay, unsigned seed = 0);
        void Cleanup();

        void OnCreate() override {}
        void OnUpdate() override;

        std::string Name() const override { return "PhysicsCollision2DTestScene"; }
    private:
        void CreateBalls(int count, unsigned seed);
        void CreateBoundaryLines();
        void UpdateBallCollisions();

        World* m_world = nullptr;
        std::vector<Entity> m_balls;
        std::vector<Entity> m_boundaryLines;

        float m_elapsedTime = 0.0f;
        float m_worldWidth = 1600.f, m_worldHeight = 900.f;
        float m_dampingDelay = 7.f;
        bool m_dampingEnabled = false;
    };
}

#endif
#endif