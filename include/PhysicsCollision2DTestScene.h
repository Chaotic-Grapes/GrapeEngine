#ifndef PHYSICSCOLLISION2DTESTSCENE_H
#define PHYSICSCOLLISION2DTESTSCENE_H

#include <vector>
#include "ecs/World.h"
#include "ecs/Entity.h"

class TestScene2D {
public:
    void Initialize(World* world, float width, float height, unsigned seed = 0);
    void Update(float deltaTime);
    void Cleanup();

    // Unity-style public properties
    float WorldWidth = 1600.0f;
    float WorldHeight = 900.0f;
    bool EnableDamping = false;
    float DampingDelay = 7.0f;

private:
    void CreateBalls(int count, unsigned seed);
    void CreateBoundaryLines();
    void UpdateBallCollisions(float deltaTime);

    World* m_world = nullptr;
    std::vector<Entity> m_balls;
    std::vector<Entity> m_boundaryLines;

    float m_elapsedTime = 0.0f;
    bool m_dampingEnabled = false;
};

#endif