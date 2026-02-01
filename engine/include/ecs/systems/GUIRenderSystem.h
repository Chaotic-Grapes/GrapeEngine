/* Start Header *****************************************************************/
/*!
\file    GUIRenderSystem.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
System for submitting GUI render commands to the renderer.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_RENDER_SYSTEM_H
#define GUI_RENDER_SYSTEM_H

#include "Export.h"
#include "ecs/ISystem.h"
#include "ecs/World.h"

namespace ECS {

    class GRAPEENGINE_API GUIRenderSystem : public ISystem {
    public:
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::PreRender; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    };

} // namespace ECS

#endif
