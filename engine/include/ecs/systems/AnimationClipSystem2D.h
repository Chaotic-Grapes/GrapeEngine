/* Start Header *****************************************************************/
/*!
\file   AnimationClipSystem2D.h
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Defines the AnimationClipSystem2D which updates frame timing and sprite data.
*/
/* End Header *******************************************************************/

#ifndef ANIMATIONCLIPSYSTEM2D_H
#define ANIMATIONCLIPSYSTEM2D_H

#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"

namespace ECS {
    /**
     * @brief The AnimationClipSystem2D is responsible for updating the current frame and sprite data 
     * of entities with AnimationController2D components based on their animation state and timing.
     * 
     * It runs in the Update phase and is only active during Play mode, as it relies on delta time and 
     * is meant to drive gameplay animations.
     */
    class AnimationClipSystem2D : public ISystem {
    public:
        AnimationClipSystem2D() = default;
        ~AnimationClipSystem2D() override = default;

        void OnCreate(World& world) override {}
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override {}

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }
    };
}

#endif
