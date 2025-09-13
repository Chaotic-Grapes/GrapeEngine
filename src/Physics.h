#ifndef PHYSICS_H
#define PHYSICS_H

#include "ISystem.h"
#include "Time.h"
#include <vector>
#include <glm/glm.hpp>

namespace Engine {

    struct PhysicsComponent {
        glm::vec3 position;
        glm::vec3 velocity;
        float damping; // damping range from 0-1, higher means more resistance

        PhysicsComponent()
            : position(0.0f, 0.0f, 0.0f)
            , velocity(0.0f, 0.0f, 0.0f)
            , damping(0.95f)
        {
        }
    };

    class PhysicsSystem : public ISystem {
    public:
        void Initialize() override;
        void Update() override;
        std::string Name() const override { return "Physics"; }

        void AddEntity(PhysicsComponent* component);
        void RemoveEntity(PhysicsComponent* component);

    private:
        void FixedUpdate();

        std::vector<PhysicsComponent*> m_entities;

        float m_accumulator = 0.0f;
        const float m_fixedTimestep = 1.0f / 60.0f; // 60 FPS physics
    };

} // namespace Engine

#endif // PHYSICS_H