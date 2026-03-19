/* Start Header *****************************************************************/
/*!
\file   PhysicsQueries2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Native C++ query API over current runtime world-shape cache.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSICSQUERIES2D_H
#define ENGINE_PHYSICS2D_PHYSICSQUERIES2D_H

#include "physics2d/PhysicsData2D.h"
#include <vector>

namespace Engine::Physics2D {
    class PhysicsQueries2D {
    public:
        /**
         * @brief Cast a ray against cached world AABBs and return nearest hit.
         * @param bodies Runtime body array.
         * @param input Ray input.
         * @return Nearest hit, or Hit=false when no intersection exists.
         */
        static RayCastHit2D RayCast(const std::vector<BodyRuntime2D>& bodies, const RayCastInput2D& input);

        /**
         * @brief Return entities whose world AABBs overlap the query AABB.
         * @param bodies Runtime body array.
         * @param query Query AABB.
         * @return Overlapping entities.
         */
        static std::vector<ECS::Entity> OverlapAABB(const std::vector<BodyRuntime2D>& bodies, const Engine::WorldAABB& query);

        /**
         * @brief Return entities whose world AABBs contain the query point.
         * @param bodies Runtime body array.
         * @param point Query point in world space.
         * @return Matching entities.
         */
        static std::vector<ECS::Entity> QueryPoint(const std::vector<BodyRuntime2D>& bodies, const Vector2D& point);
    };
}

#endif
