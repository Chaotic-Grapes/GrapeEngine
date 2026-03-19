/* Start Header *****************************************************************/
/*!
\file   PhysicsPipelineRunner.h
\author Dalton Koh Shi Hao (100%)
\par    d.koh@digipen.edu

\brief
Declares the internal parity-first physics pipeline entry point used by
PhysicsSystem::OnUpdate.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef ECS_INTERNAL_PHYSICS_PIPELINE_RUNNER_H
#define ECS_INTERNAL_PHYSICS_PIPELINE_RUNNER_H

namespace Scenes {
    class LayerManager;
}

namespace ECS {
    class PhysicsSystem;
    class World;

    /**
     * @brief Execute one full physics frame using the internal staged pipeline.
     *
     * @param system Physics system instance owning persistent collision caches.
     * @param world ECS world containing simulation components.
     * @param layerManager Layer manager used for layer gating and collision masks.
     * @param dt Frame delta time in seconds.
     */
    void RunPhysicsPipeline(PhysicsSystem& system, World& world, Scenes::LayerManager& layerManager, float dt);
}

#endif  // ECS_INTERNAL_PHYSICS_PIPELINE_RUNNER_H
