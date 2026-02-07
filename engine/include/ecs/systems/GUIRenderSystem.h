/* Start Header *****************************************************************/
/*!
\file   GUIRenderSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Declaration of the GUIRenderSystem class for rendering GUI elements. Provides
functionality for translating ECS GUI components into draw calls.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/ISystem.h"

namespace ECS {
    class GUIRenderSystem : public ISystem {
    public:
        // Initialize GUI render system state for the world.
        void OnCreate(World& world) override;
        // Build GUI draw data and issue render calls.
        void OnUpdate(World& world) override;
        // Tear down GUI render system state.
        void OnDestroy(World& world) override;

        // Return metadata used for system registration.
        SystemMetadata GetMetadata() const override;
        // Run alongside rendering systems.
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        // Always run to keep GUI rendering up to date.
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    };
}
