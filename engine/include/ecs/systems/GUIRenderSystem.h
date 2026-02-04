#pragma once

#include "ecs/ISystem.h"

namespace ECS {
    class GUIRenderSystem : public ISystem {
    public:
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    };
}
