/* Start Header *****************************************************************/
/*!
\file   Broadphase2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic dynamic AABB-tree broadphase pair generation.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_BROADPHASE2D_H
#define ENGINE_PHYSICS2D_BROADPHASE2D_H

#include "physics2d/PhysicsData2D.h"
#include <vector>

namespace Engine::Physics2D {
    class Broadphase2D {
    public:
        /**
         * @brief Rebuild the dynamic AABB tree from the current body cache.
         * @param bodies Runtime body array.
         * @param fatPadding Extra half-extent padding used to reduce tree churn.
         */
        void Build(const std::vector<BodyRuntime2D>& bodies, float fatPadding);

        /**
         * @brief Generate sorted unique broadphase pairs using deterministic static partitioning.
         * @param bodies Runtime body array.
         * @return Candidate collider pairs for narrowphase.
         */
        std::vector<BroadphasePair2D> GeneratePairsDeterministicParallel(const std::vector<BodyRuntime2D>& bodies) const;

    private:
        /**
         * @brief Node for dynamic AABB tree broadphase.
         */
        struct TreeNode {
            Engine::WorldAABB Box{};
            int Parent = -1;
            int Left = -1;
            int Right = -1;
            size_t BodyIndex = static_cast<size_t>(-1);
            bool IsLeaf = false;
        };

        /**
         * @brief Return an AABB that encloses both inputs.
         */
        static Engine::WorldAABB Merge(const Engine::WorldAABB& a, const Engine::WorldAABB& b);

        /**
         * @brief Test overlap between two AABBs.
         */
        static bool Overlap(const Engine::WorldAABB& a, const Engine::WorldAABB& b);

        /**
         * @brief Cost heuristic used by leaf insertion.
         */
        static float Perimeter(const Engine::WorldAABB& a);

        /**
         * @brief Insert one fattened body AABB into the tree.
         */
        void InsertLeaf(size_t bodyIndex, const Engine::WorldAABB& fatAabb);

        /**
         * @brief Collect body indices whose tree nodes overlap the query AABB.
         */
        void QueryOverlaps(const Engine::WorldAABB& query, std::vector<size_t>& outBodies) const;

        std::vector<TreeNode> m_nodes;
        int m_root = -1;
    };
}

#endif
