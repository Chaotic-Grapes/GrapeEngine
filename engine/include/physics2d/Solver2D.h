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
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine::Physics2D {
    /**
     * @brief Deterministic sequential-impulse solver with warm-start cache.
     */
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
            std::vector<ContactConstraint2D>& contacts,
            const std::vector<PhysicsIsland2D>& islands);

    private:
        /**
         * @brief Stable key used to identify persisted contact impulses.
         */
        struct PairKey {
            PackedEntityId A = 0;
            PackedEntityId B = 0;
            uint32_t FeatureId = 0;

            /**
             * @brief Compare two pair keys for hash-map equality.
             * @param other Key to compare against.
             * @return `true` when both keys refer to the same contact feature.
             */
            bool operator==(const PairKey& other) const {
                return A == other.A && B == other.B && FeatureId == other.FeatureId;
            }
        };

        /**
         * @brief Hash functor for `PairKey`.
         */
        struct PairKeyHash {
            /**
             * @brief Hash a contact pair key for warm-start cache lookup.
             * @param pair Pair key value.
             * @return Hash value.
             */
            size_t operator()(const PairKey& pair) const noexcept {
                const size_t h1 = std::hash<uint64_t>{}(pair.A);
                const size_t h2 = std::hash<uint64_t>{}(pair.B);
                const size_t h3 = std::hash<uint32_t>{}(pair.FeatureId);
                const size_t k = h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
                return k ^ (h3 + 0x9e3779b97f4a7c15ull + (k << 6) + (k >> 2));
            }
        };

        /**
         * @brief Cached normal/tangent impulses for warm starting.
         */
        struct WarmImpulseCacheEntry {
            float NormalImpulse = 0.0f;
            float TangentImpulse = 0.0f;
        };

        /**
         * @brief Solve all contacts belonging to a single island.
         * @param config Physics tuning values.
         * @param bodies Runtime body cache.
         * @param contacts Contact constraints.
         * @param island Island to solve.
         * @param invMass Per-body inverse mass cache.
         * @param friction Per-body friction cache.
         * @param restitution Per-body restitution cache.
         * @param linearVelocity Per-body linear velocity SoA buffer.
         */
        static void SolveIslandVelocity(
            const PhysicsConfig2D& config,
            std::vector<ContactConstraint2D>& contacts,
            const PhysicsIsland2D& island,
            const std::vector<float>& invMass,
            const std::vector<float>& friction,
            const std::vector<float>& restitution,
            std::vector<Vector2D>& linearVelocity);

        /**
         * @brief Solve position correction for one island.
         * @param config Physics tuning values.
         * @param bodies Runtime body cache.
         * @param contacts Contact constraints.
         * @param island Island to solve.
         * @param invMass Per-body inverse mass cache.
         * @param positionPercent Per-body position correction factor.
         * @param positions Per-body position SoA buffer.
         */
        static void SolveIslandPosition(
            const PhysicsConfig2D& config,
            const std::vector<BodyRuntime2D>& bodies,
            const std::vector<ContactConstraint2D>& contacts,
            const PhysicsIsland2D& island,
            const std::vector<float>& invMass,
            const std::vector<float>& positionPercent,
            std::vector<Vector2D>& positions);

        std::unordered_map<PairKey, WarmImpulseCacheEntry, PairKeyHash> m_warmStartCache;
    };
}

#endif
