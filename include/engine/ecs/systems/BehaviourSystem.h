//#ifndef BEHAVIOURSYSTEM_H
//#define BEHAVIOURSYSTEM_H
//
//#include <memory>
//#include <vector>
//#include "ecs/Behaviour.h"
//#include "ecs/ISystem.h"
//#include "ecs/World.h"
//#include "services/Time.h"
//
//class BehaviourSystem : public Engine::ISystem {
//public:
//    explicit BehaviourSystem(World* world) : m_world(world) {}
//
//    void OnCreate() override {}
//
//    void OnUpdate() override;
//
//private:
//    World* m_world;
//    struct TrackedBehaviour {
//        std::unique_ptr<Behaviour> m_instance;
//        bool m_enabled = true;
//        bool m_started = false;
//    };
//
//    std::vector<TrackedBehaviour> m_behaviours;
//    float m_fixedTimeAccumulator = 0.f;
//};
//
//#endif
