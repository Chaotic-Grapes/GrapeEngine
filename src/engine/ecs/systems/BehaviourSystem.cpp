//#include "ecs/systems/BehaviourSystem.h"
//
//void BehaviourSystem::OnUpdate() {
//    const auto& fixedDt = Time::FixedDeltaTime();
//
//    // Call Start() if not started
//    for (auto& b : m_behaviours) {
//        if (!b.m_started) {
//            b.m_instance->Start();
//            b.m_started = true;
//        }
//    }
//
//    // Per-frame updates
//    for (const auto& b : m_behaviours) {
//        if (b.m_enabled) {
//            b.m_instance->Update();
//            b.m_instance->LateUpdate();
//        }
//    }
//
//    // Fixed updates
//    m_fixedTimeAccumulator += Time::DeltaTime();
//    while (m_fixedTimeAccumulator >= fixedDt) {
//        for (const auto& b : m_behaviours)
//            if (b.m_enabled) b.m_instance->FixedUpdate();
//        m_fixedTimeAccumulator -= fixedDt;
//    }
//}