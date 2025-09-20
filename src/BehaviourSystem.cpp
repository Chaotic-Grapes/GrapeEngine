#include "systems/BehaviourSystem.h"

void BehaviourSystem::OnUpdate() {
    const auto& fixedDt = Time::FixedDeltaTime();

    // Call Start() if not started
    for (auto& b : m_behaviours) {
        if (!b.m_started) {
            b.m_instance->Start();
            b.m_started = true;
        }
    }

    // Per-frame updates
    for (const auto& b : m_behaviours) {
        if (b.m_enabled) {
            b.m_instance->Update();
            b.m_instance->LateUpdate();
        }
    }

    // Fixed updates
    m_fixedTimeAccumulator += Time::DeltaTime();
    while (m_fixedTimeAccumulator >= fixedDt) {
        for (const auto& b : m_behaviours)
            if (b.m_enabled) b.m_instance->FixedUpdate();
        m_fixedTimeAccumulator -= fixedDt;
    }
}

void BehaviourSystem::AddBehaviour(std::unique_ptr<Behaviour> behaviour) {
    TrackedBehaviour tb;
    tb.m_instance = std::move(behaviour);
    tb.m_enabled = true;
    tb.m_started = false;
    tb.m_instance->Awake();
    tb.m_instance->OnEnable();
    m_behaviours.push_back(std::move(tb));
}

void BehaviourSystem::RemoveBehaviour(Behaviour* behaviour) {
// Find and remove the behaviour
    const auto it = std::remove_if(m_behaviours.begin(), m_behaviours.end(),
        [behaviour](const TrackedBehaviour& b) {
            return b.m_instance.get() == behaviour;
        });

// Call OnDisable and OnDestroy before removing
    for (auto i = it; i != m_behaviours.end(); ++i) {
        i->m_instance->OnDisable();
        i->m_instance->OnDestroy();
    }

    m_behaviours.erase(it, m_behaviours.end());
}