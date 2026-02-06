/* Start Header *****************************************************************/
/*!
\file    GUILayoutSystem.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the GUILayoutSystem which is responsible
for calculating the layout of GUI elements based on their properties and the
current viewport.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#pragma once

#include "ecs/ISystem.h"

namespace ECS {
    class GUILayoutSystem : public ISystem {
    public:
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    };
}
