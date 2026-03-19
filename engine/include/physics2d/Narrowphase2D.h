/* Start Header *****************************************************************/
/*!
\file   Narrowphase2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Narrowphase manifold generation for circle/box collider pairs.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_NARROWPHASE2D_H
#define ENGINE_PHYSICS2D_NARROWPHASE2D_H

#include "physics2d/PhysicsData2D.h"
#include <vector>

namespace Engine::Physics2D {
    class Narrowphase2D {
    public:
        /**
         * @brief Generate contact constraints from broadphase pairs in deterministic order.
         * @param bodies Runtime body array.
         * @param pairs Candidate broadphase pairs.
         * @return Contacts ready for solver/event processing.
         */
        std::vector<ContactConstraint2D> BuildContactsParallel(
            const std::vector<BodyRuntime2D>& bodies,
            const std::vector<BroadphasePair2D>& pairs) const;

        /**
         * @brief Build a manifold for a single shape pair.
         * @param a First body.
         * @param b Second body.
         * @param out Output manifold.
         * @return True when bodies overlap.
         */
        static bool BuildManifold(const BodyRuntime2D& a, const BodyRuntime2D& b, Engine::Collision::ContactManifold& out);
    };
}

#endif
