/* Start Header *****************************************************************/
/*!
\file    Scene.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (95%)
         Foo Rui Qin (5%)
\date    12th March 2026
\par     muhammadnurfadzly.b@digipen.edu
         ruiqin.foo@digipen.edu
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
    // Create an entity on a target layer while enforcing sane defaults for editor and runtime flows
    ECS::Entity Scene::CreateEntityOnLayer(const uint16_t layerId, const std::optional<ECS::Entity> parent) {
        // Create the raw entity first, then initialize baseline components explicitly
        const ECS::Entity e = m_world.Create();

        // New entities default to active so they participate in rendering and system updates immediately
        // User can disable after creation if desired
        m_world.Set<ECS::Components::Active>(e, ECS::Components::Active{ true });

        // Only attach when parent is both provided and alive to avoid writing invalid hierarchy links
        if (parent.has_value() && m_world.IsAlive(parent.value())) {
            m_world.Attach(e, parent.value());
        }

        // Apply layer assignment through the centralized path so layer bookkeeping remains consistent
        SetLayer(e, layerId);
        return e;
    }

    // Validate entity liveness before forwarding to SetLayer to avoid stale-handle writes
    void Scene::SetLayerById(const ECS::Entity e, const uint16_t id) {
        if (e.IsNull() || !m_world.IsAlive(e)) {
            return;
        }

        SetLayer(e, id);
    }
}
