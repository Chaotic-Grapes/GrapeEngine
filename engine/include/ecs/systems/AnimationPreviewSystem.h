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

        void OnCreate(World& world) override {}
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override {}

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::EditOnly; }
    };
}

#endif
