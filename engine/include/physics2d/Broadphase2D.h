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
#include <unordered_map>
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
         * @brief Incrementally update tree proxies when topology is unchanged.
         * @param bodies Runtime body array.
         * @param fatPadding Extra half-extent padding used to reduce tree churn.
         */
        void Update(const std::vector<BodyRuntime2D>& bodies, float fatPadding);

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
         * @param a First AABB.
         * @param b Second AABB.
         * @return Enclosing AABB.
         */
        static Engine::WorldAABB Merge(const Engine::WorldAABB& a, const Engine::WorldAABB& b);

        /**
         * @brief Test overlap between two AABBs.
         * @param a First AABB.
         * @param b Second AABB.
         * @return True when AABBs overlap.
         */
        static bool Overlap(const Engine::WorldAABB& a, const Engine::WorldAABB& b);

        /**
         * @brief Cost heuristic used by leaf insertion.
         * @param a Candidate AABB.
         * @return Heuristic perimeter value.
         */
        static float Perimeter(const Engine::WorldAABB& a);

        /**
         * @brief Insert one fattened body AABB into the tree.
         * @param bodyIndex Runtime body index.
         * @param fatAabb Expanded body AABB.
         */
        void InsertLeaf(size_t bodyIndex, const Engine::WorldAABB& fatAabb);

        /**
         * @brief Collect body indices whose tree nodes overlap the query AABB.
         * @param query Query AABB.
         * @param outBodies Output overlapping body indices.
         */
        void QueryOverlaps(const Engine::WorldAABB& query, std::vector<size_t>& outBodies) const;

        /**
         * @brief Test whether one AABB fully contains another.
         * @param outer Candidate container.
         * @param inner Candidate contained AABB.
         * @return True when `inner` is fully inside `outer`.
         */
        static bool Contains(const Engine::WorldAABB& outer, const Engine::WorldAABB& inner);

        /**
         * @brief Expand an AABB by symmetric fat padding.
         * @param source Source AABB.
         * @param fatPadding Expansion amount per axis side.
         * @return Expanded AABB.
         */
        static Engine::WorldAABB ExpandFat(const Engine::WorldAABB& source, float fatPadding);

        /**
         * @brief Recompute enclosing boxes while walking parent links to root.
         * @param nodeIndex Starting node index.
         */
        void RefitAncestors(int nodeIndex);

        /**
         * @brief Decide whether incremental update should fallback to full rebuild.
         * @param bodies Runtime body array.
         * @param fatPadding Current fat AABB padding.
         * @return True when topology/padding drift requires rebuilding the tree.
         */
        bool RequiresRebuild(const std::vector<BodyRuntime2D>& bodies, float fatPadding);

        struct ProxyBinding {
            PackedEntityId Packed = 0;
            size_t BodyIndex = 0;
            int NodeIndex = -1;
        };

        std::vector<TreeNode> m_nodes;
        std::vector<ProxyBinding> m_bindings;
        std::unordered_map<PackedEntityId, size_t> m_bindingByPacked;
        int m_root = -1;
        float m_fatPadding = 0.0f;
        uint32_t m_stepsSinceRebuild = 0;
    };
}

#endif
