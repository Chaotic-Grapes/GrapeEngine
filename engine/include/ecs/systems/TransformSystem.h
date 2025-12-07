/* Start Header *****************************************************************/
/*!
\file   TransformSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares the TransformSystem which updates world transforms for entity hierarchies.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef TRANSFORMSYSTEM_H
#define TRANSFORMSYSTEM_H

#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/ISystem.h"
#include "math/Matrix4x4.h"
#include "helpers/TransformUtils.h"
#include <vector>
#include <optional>

namespace ECS {
    /**
     * @brief System that updates WorldTransform components from LocalTransform
     * and the entity parent hierarchy. Runs in PrePhysics so physics/render
     * have up-to-date world matrices.
     */
    class TransformSystem : public ISystem {
    public:
        TransformSystem() = default;
        ~TransformSystem() override = default;

        void OnCreate(World& world) override {}
        void OnUpdate(World& world, float dt) override;
        void OnDestroy(World& world) override {}

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::PrePhysics; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    private:
        void _updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld);
    };
}

#endif
