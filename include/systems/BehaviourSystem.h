#ifndef BEHAVIOURSYSTEM_H
#define BEHAVIOURSYSTEM_H

#include <memory>
#include <vector>
#include "Behaviour.h"
#include "ecs/ISystem.h"
#include "systems/Time.h"

class BehaviourSystem : public Engine::ISystem {
public:
    void OnCreate() override {}

    void OnUpdate() override;

    void AddBehaviour(std::unique_ptr<Behaviour> behaviour);

    void RemoveBehaviour(Behaviour* behaviour);

private:
    struct TrackedBehaviour {
        std::unique_ptr<Behaviour> m_instance;
        bool m_enabled = true;
        bool m_started = false;
    };

    std::vector<TrackedBehaviour> m_behaviours;
    float m_fixedTimeAccumulator = 0.f;
};

#endif
