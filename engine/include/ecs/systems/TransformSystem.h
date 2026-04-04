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
#include "ecs/ComponentAccessAttribute.h"
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

        /**
         * @brief No-op; transform system requires no per-world initialization.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnCreate(World& world) override { (void)world; }

        /**
         * @brief Propagate LocalTransform changes through the entity hierarchy to update WorldTransform.
         * @param world ECS world containing entities with transform components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief No-op; transform system holds no per-world resources to release.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnDestroy(World& world) override { (void)world; }

        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Run in PrePhysics so world transforms are valid before physics and rendering.
         * @return SystemGroup::PrePhysics.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::PrePhysics; }

        /**
         * @brief Always run in both play and edit modes.
         * @return SystemRunMode::Always.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

    private:
        /**
         * @brief Recursively update the WorldTransform for an entity and all its children.
         * @param world ECS world containing the entity hierarchy.
         * @param e Entity whose subtree to update.
         * @param parentWorld Parent's world matrix, or nullopt for root entities.
         */
        void _updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld);
    };
}

#endif
