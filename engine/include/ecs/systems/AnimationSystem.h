/* Start Header *****************************************************************/
/*!
\file   AnimationSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   1st November 2025
\brief
Defines the AnimationSystem which is responsible for updating sprite sheet
animations in the ECS framework.

The system processes entities with SpriteSheetAnimation2D and AnimationState2D
components, advancing frames based on elapsed time and updating the associated
SpriteRenderer2D component with the correct UV coordinates for the current frame.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ANIMATIONSYSTEM_H
#define ANIMATIONSYSTEM_H

#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"

namespace ECS {
    /**
     * @brief System that updates sprite sheet animations
     * Executes in Update phase with executionOrder=200
     */
    class AnimationSystem : public ISystem {
    public:
        AnimationSystem() = default;
        ~AnimationSystem() override = default;

        /**
         * @brief No-op; animation system requires no per-world initialization.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnCreate(World& world) override { (void)world; }

        /**
         * @brief Advance sprite sheet animations for all active entities this frame.
         * @param world ECS world containing entities with animation components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief No-op; animation system holds no per-world resources to release.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnDestroy(World& world) override { (void)world; }

        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Run in the Update group after game logic.
         * @return SystemGroup::Update.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }

        /**
         * @brief Only run during play mode; animations do not advance in edit mode.
         * @return SystemRunMode::PlayOnly.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }
    };
}

#endif
