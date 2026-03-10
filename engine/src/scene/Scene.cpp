/* Start Header *****************************************************************/
/*!
\file    Scene.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Out-of-line Scene helpers that avoid cross-module component ID mismatches.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "scene/Scene.h"

namespace Scenes {
    ECS::Entity Scene::CreateEntityOnLayer(const uint16_t layerId, const std::optional<ECS::Entity> parent) {
        const ECS::Entity e = m_world.Create();

		// Set Active by default so new entities are enabled
        // User can disable after creation if desired
        m_world.Set<ECS::Components::Active>(e, ECS::Components::Active{ true });
        if (parent.has_value() && m_world.IsAlive(parent.value())) {
            m_world.Attach(e, parent.value());
        }

        SetLayer(e, layerId);
        return e;
    }

    void Scene::SetLayerById(const ECS::Entity e, const uint16_t id) {
        if (e.IsNull() || !m_world.IsAlive(e)) {
            return;
        }

        SetLayer(e, id);
    }
}
