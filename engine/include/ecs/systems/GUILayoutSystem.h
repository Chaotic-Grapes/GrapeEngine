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
        /**
         * @brief Initialize system state when the world is created.
         * @param world ECS world used to query viewport and GUI components.
         */
        void OnCreate(World& world) override;

        /**
         * @brief Calculate and apply GUI layout for all active GUI entities this frame.
         * @param world ECS world containing entities with GUI layout components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief Tear down system state before shutdown.
         * @param world ECS world passed by the scheduler.
         */
        void OnDestroy(World& world) override;

        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Run alongside rendering systems in the Render group.
         * @return SystemGroup::Render.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }

        /**
         * @brief Always run to keep GUI layout up to date regardless of play/edit mode.
         * @return SystemRunMode::Always.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }
    };
}
