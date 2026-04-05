/* Start Header *****************************************************************/
/*!
\file   AnimationPreviewSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th January 2026
\brief
Defines the AnimationPreviewSystem which updates sprite UVs for animation preview
in editor mode without advancing time.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ANIMATIONPREVIEWSYSTEM_H
#define ANIMATIONPREVIEWSYSTEM_H

#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"

namespace ECS {
    /**
     * @brief System that updates sprite sheet UVs for editor preview
     * Executes in Update phase with executionOrder=190 in EditOnly mode
     */
    class AnimationPreviewSystem : public ISystem {
    public:
        AnimationPreviewSystem() = default;
        ~AnimationPreviewSystem() override = default;

        /**
         * @brief No-op; preview system requires no per-world initialization.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnCreate(World& world) override { (void)world; }

        /**
         * @brief Update sprite UVs to reflect the current preview frame without advancing time.
         * @param world ECS world containing entities with animation components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief No-op; preview system holds no per-world resources to release.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnDestroy(World& world) override { (void)world; }

        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Run in the Update group.
         * @return SystemGroup::Update.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }

        /**
         * @brief Only run in edit mode; preview is not needed during play.
         * @return SystemRunMode::EditOnly.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::EditOnly; }
    };
}

#endif
