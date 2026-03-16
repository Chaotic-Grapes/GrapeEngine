/* Start Header *****************************************************************/
/*!
\file   PhysicsWorld2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Fixed-step deterministic 2D physics coordinator.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSICSWORLD2D_H
#define ENGINE_PHYSICS2D_PHYSICSWORLD2D_H

#include "physics2d/PhysicsConfig2D.h"
#include "physics2d/PhysicsStats2D.h"
#include "physics2d/PhysicsDebugDraw2D.h"
#include "physics2d/PhysicsData2D.h"
#include "physics2d/Broadphase2D.h"
#include "physics2d/Narrowphase2D.h"
#include "physics2d/ContactManager2D.h"
#include "physics2d/IslandBuilder2D.h"
#include "physics2d/Solver2D.h"
#include "physics2d/CCD2D.h"
#include "physics2d/PhysicsQueries2D.h"

namespace ECS {
    class World;
}
namespace Scenes {
    class LayerManager;
}
namespace ECS::Events {
    class EventDispatcher;
}

namespace Engine::Physics2D {
    class PhysicsWorld2D {
    public:
        /**
         * @brief Construct a world with default physics configuration.
         */
        PhysicsWorld2D() = default;

        /**
         * @brief Execute one fixed physics step.
         * @param world ECS world.
         * @param layerManager Layer settings used by filtering.
         * @param dispatcher Event dispatcher for physics events.
         * @param fixedDt Unused external fixed dt (runtime uses Config().FixedDeltaSeconds).
         * @param tilemaps Optional tilemap collision proxies.
         */
        void StepFixed(
            ECS::World& world,
            Scenes::LayerManager& layerManager,
            ECS::Events::EventDispatcher& dispatcher,
            float fixedDt,
            const std::vector<TilemapCollisionProxy2D>& tilemaps);

        /**
         * @brief Access last frame statistics.
         */
        const PhysicsStats2D& GetStats() const { return m_stats; }

        /**
         * @brief Mutable physics configuration.
         */
        PhysicsConfig2D& Config() { return m_config; }

        /**
         * @brief Runtime body cache from the most recent step.
         */
        const std::vector<BodyRuntime2D>& Bodies() const { return m_bodies; }

        /**
         * @brief Set debug draw callback, or null to disable.
         */
        void SetDebugDraw(IPhysicsDebugDraw2D* debugDraw) { m_debugDraw = debugDraw; }

    private:
        /**
         * @brief Pull relevant ECS components into runtime body cache.
         */
        void SyncFromECS(ECS::World& world, Scenes::LayerManager& layerManager);

        /**
         * @brief Recompute world-space collider representations for cached bodies.
         */
        void BuildWorldShapes(ECS::World& world);

        /**
         * @brief Integrate dynamic body velocities and transforms.
         */
        void IntegrateDynamic(float fixedDt);

        /**
         * @brief Resolve dynamic-vs-tilemap collisions.
         */
        void ResolveTilemaps(ECS::World& world, Scenes::LayerManager& layerManager, const std::vector<TilemapCollisionProxy2D>& tilemaps);

        /**
         * @brief Advance sleep timers and sleeping flags.
         */
        void UpdateSleepState(float fixedDt);

        /**
         * @brief Publish physics runtime state back into ECS components/tags.
         */
        void WriteBackToECS(ECS::World& world);

        PhysicsConfig2D m_config{};
        PhysicsStats2D m_stats{};
        Broadphase2D m_broadphase{};
        Narrowphase2D m_narrowphase{};
        ContactManager2D m_contactManager{};
        IslandBuilder2D m_islandBuilder{};
        Solver2D m_solver{};
        CCD2D m_ccd{};

        std::vector<BodyRuntime2D> m_bodies;
        std::vector<BroadphasePair2D> m_pairs;
        std::vector<ContactConstraint2D> m_contacts;
        std::vector<PhysicsIsland2D> m_islands;
        IPhysicsDebugDraw2D* m_debugDraw = nullptr;
    };
}

#endif
