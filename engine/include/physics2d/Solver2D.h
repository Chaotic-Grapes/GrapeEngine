/* Start Header *****************************************************************/
/*!
\file   Solver2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Sequential impulse solver for deterministic 2D rigid-body response.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_SOLVER2D_H
#define ENGINE_PHYSICS2D_SOLVER2D_H

#include "physics2d/PhysicsConfig2D.h"
#include "physics2d/PhysicsData2D.h"
#include "physics2d/IslandBuilder2D.h"
#include <vector>

namespace Engine::Physics2D {
    class Solver2D {
    public:
        /**
         * @brief Solve non-trigger contacts per island using sequential impulses.
         * @param config Physics tuning values.
         * @param bodies Runtime body cache.
         * @param contacts Contact constraints.
         * @param islands Deterministic connected components of contacts.
         */
        void Solve(
            const PhysicsConfig2D& config,
            std::vector<BodyRuntime2D>& bodies,
            const std::vector<ContactConstraint2D>& contacts,
            const std::vector<PhysicsIsland2D>& islands) const;

    private:
        /**
         * @brief Solve all contacts belonging to a single island.
         */
        static void SolveIsland(
            const PhysicsConfig2D& config,
            std::vector<BodyRuntime2D>& bodies,
            const std::vector<ContactConstraint2D>& contacts,
            const PhysicsIsland2D& island);
    };
}

#endif
