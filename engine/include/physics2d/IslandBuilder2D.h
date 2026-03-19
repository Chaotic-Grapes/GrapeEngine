/* Start Header *****************************************************************/
/*!
\file   IslandBuilder2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic contact-graph island builder.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_ISLANDBUILDER2D_H
#define ENGINE_PHYSICS2D_ISLANDBUILDER2D_H

#include "physics2d/PhysicsData2D.h"
#include <vector>

namespace Engine::Physics2D {
    /**
     * @brief Connected subset of bodies and contacts solved together.
     */
    struct PhysicsIsland2D {
        std::vector<size_t> Bodies;
        std::vector<size_t> Contacts;
    };

    class IslandBuilder2D {
    public:
        /**
         * @brief Build deterministic connected components over non-trigger contacts.
         * @param bodies Runtime body array.
         * @param contacts Contact constraints for the current frame.
         * @return Islands with sorted body and contact indices.
         */
        std::vector<PhysicsIsland2D> Build(
            const std::vector<BodyRuntime2D>& bodies,
            const std::vector<ContactConstraint2D>& contacts) const;
    };
}

#endif
