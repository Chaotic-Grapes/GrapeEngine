/* Start Header *****************************************************************/
/*!
\file   Solver2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Sequential impulse solver with warm starting and split position pass.
\references
- https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf
- https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf
- https://box2d.org/documentation/
- https://myphysicslab.com/engine2D/collision-en.html
*/
/* End Header *******************************************************************/

#include "physics2d/Solver2D.h"
#include "physics2d/internal/ParallelFor.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

namespace {
    /**
     * @brief Compute 2D dot product.
     * @param a First vector.
     * @param b Second vector.
     * @return Dot product.
     */
    static float Dot2D(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    /**
     * @brief Resolve inverse mass for runtime body logic.
     * @param rb Rigidbody pointer (can be null).
     * @return Inverse mass, or `0` for static/invalid bodies.
     */
    static float InvMassOf(const ECS::Components::Rigidbody2D* rb) {
        if (!rb || rb->Mass <= 0.0f) {
            return 0.0f;
        }
        return (rb->InverseMass > 0.0f) ? rb->InverseMass : (1.0f / rb->Mass);
    }

    /**
     * @brief Check whether a runtime body has writable transform state.
     * @param b Runtime body.
     * @return `true` when local transform exists.
     */
    static bool HasPosition(const Engine::Physics2D::BodyRuntime2D& b) {
        return b.Local != nullptr;
    }

#if !defined(NDEBUG)
    /**
     * @brief Debug-only validation for disjoint island ownership.
     * @param bodyCount Total body count.
     * @param contactCount Total contact count.
     * @param islands Ordered island list.
     * @return None.
     */
    static void ValidateIslandOwnership(
        size_t bodyCount,
        size_t contactCount,
        const std::vector<Engine::Physics2D::PhysicsIsland2D>& islands)
    {
        std::vector<int32_t> bodyOwner(bodyCount, -1);
        std::vector<int32_t> contactOwner(contactCount, -1);
        for (size_t islandIdx = 0; islandIdx < islands.size(); ++islandIdx) {
            const auto& island = islands[islandIdx];
            for (size_t b : island.Bodies) {
                assert(b < bodyCount);
                assert(bodyOwner[b] == -1);
                bodyOwner[b] = static_cast<int32_t>(islandIdx);
            }
            for (size_t c : island.Contacts) {
                assert(c < contactCount);
                assert(contactOwner[c] == -1);
                contactOwner[c] = static_cast<int32_t>(islandIdx);
            }
        }
    }
#endif
}

namespace Engine::Physics2D {
    /**
     * @brief Solve velocity impulses for a single island.
     * @param config Physics tuning values.
     * @param contacts Contact constraints.
     * @param island Island to solve.
     * @param invMass Per-body inverse mass cache.
     * @param friction Per-body friction cache.
     * @param restitution Per-body restitution cache.
     * @param linearVelocity Per-body linear velocity buffer.
     */
    void Solver2D::SolveIslandVelocity(
        const PhysicsConfig2D& config,
        std::vector<ContactConstraint2D>& contacts,
        const PhysicsIsland2D& island,
        const std::vector<float>& invMass,
        const std::vector<float>& friction,
        const std::vector<float>& restitution,
        std::vector<Vector2D>& linearVelocity)
    {
        // Multiple iterations improve convergence of coupled constraints.
        for (uint32_t it = 0; it < config.VelocityIterations; ++it) {
            for (size_t cidx : island.Contacts) {
                if (cidx >= contacts.size()) {
                    continue;
                }
                ContactConstraint2D& c = contacts[cidx];
                if (c.IsTrigger) {
                    continue;
                }

                const float invMassA = invMass[c.BodyA];
                const float invMassB = invMass[c.BodyB];
                const float invMassSum = invMassA + invMassB;
                if (invMassSum <= 0.0f) {
                    // Nothing to move if both bodies are effectively static.
                    continue;
                }

                Vector2D vA = linearVelocity[c.BodyA];
                Vector2D vB = linearVelocity[c.BodyB];
                const Vector2D n = c.Manifold.normal;
                const Vector2D rv = vB - vA;
                const float vn = Dot2D(rv, n);

                // Normal impulse handles restitution and blocks interpenetrating velocity.
                const float e = std::max(restitution[c.BodyA], restitution[c.BodyB]);
                float lambdaN = -(vn * (1.0f + e)) / invMassSum;
                const float oldNormalImpulse = c.NormalImpulse;
                // Keep accumulated normal impulse non-negative (no attractive impulses).
                c.NormalImpulse = std::max(oldNormalImpulse + lambdaN, 0.0f);
                lambdaN = c.NormalImpulse - oldNormalImpulse;
                const Vector2D impulseN = n * lambdaN;
                vA -= impulseN * invMassA;
                vB += impulseN * invMassB;

                const Vector2D rvPost = vB - vA;
                Vector2D tangent = rvPost - n * Dot2D(rvPost, n);
                const float tLenSq = Dot2D(tangent, tangent);
                if (tLenSq > 1e-8f) {
                    // Tangential impulse is clamped by Coulomb friction cone.
                    tangent = tangent / std::sqrt(tLenSq);
                    const float mu = std::clamp((friction[c.BodyA] + friction[c.BodyB]) * 0.5f, 0.0f, 1.0f);
                    float lambdaT = -Dot2D(rvPost, tangent) / invMassSum;
                    const float maxT = mu * c.NormalImpulse;
                    const float oldTangentImpulse = c.TangentImpulse;
                    // Friction is limited by |jt| <= mu * jn.
                    c.TangentImpulse = std::clamp(oldTangentImpulse + lambdaT, -maxT, maxT);
                    lambdaT = c.TangentImpulse - oldTangentImpulse;
                    const Vector2D impulseT = tangent * lambdaT;
                    vA -= impulseT * invMassA;
                    vB += impulseT * invMassB;
                }

                linearVelocity[c.BodyA] = vA;
                linearVelocity[c.BodyB] = vB;
            }
        }
    }

    /**
     * @brief Solve positional penetration correction for a single island.
     * @param config Physics tuning values.
     * @param bodies Runtime body cache.
     * @param contacts Contact constraints.
     * @param island Island to solve.
     * @param invMass Per-body inverse mass cache.
     * @param positionPercent Per-body position-correction percent cache.
     * @param positions Per-body position buffer.
     */
    void Solver2D::SolveIslandPosition(
        const PhysicsConfig2D& config,
        const std::vector<BodyRuntime2D>& bodies,
        const std::vector<ContactConstraint2D>& contacts,
        const PhysicsIsland2D& island,
        const std::vector<float>& invMass,
        const std::vector<float>& positionPercent,
        std::vector<Vector2D>& positions)
    {
        // Split impulse style position pass to reduce persistent penetration drift.
        for (uint32_t it = 0; it < config.PositionIterations; ++it) {
            for (size_t cidx : island.Contacts) {
                if (cidx >= contacts.size()) {
                    continue;
                }
                const ContactConstraint2D& c = contacts[cidx];
                if (c.IsTrigger) {
                    continue;
                }
                if (!HasPosition(bodies[c.BodyA]) || !HasPosition(bodies[c.BodyB])) {
                    continue;
                }

                const float invMassA = invMass[c.BodyA];
                const float invMassB = invMass[c.BodyB];
                const float invMassSum = invMassA + invMassB;
                if (invMassSum <= 0.0f) {
                    continue;
                }

                const float percent = std::clamp((positionPercent[c.BodyA] + positionPercent[c.BodyB]) * 0.5f,
                    0.0f, 1.0f);
                const float correctionMagnitude = std::max(c.Manifold.penetration - config.ContactSlop, 0.0f) * percent;
                if (correctionMagnitude <= 0.0f) {
                    continue;
                }

                // Move bodies apart along contact normal proportional to inverse mass.
                const Vector2D correction = c.Manifold.normal * (correctionMagnitude / invMassSum);
                positions[c.BodyA] -= correction * invMassA;
                positions[c.BodyB] += correction * invMassB;
            }
        }
    }

    /**
     * @brief Solve all collision islands and refresh warm-start cache.
     * @param config Physics tuning values.
     * @param bodies Runtime body cache.
     * @param contacts Contact constraints.
     * @param islands Deterministic island list.
     */
    void Solver2D::Solve(
        const PhysicsConfig2D& config,
        std::vector<BodyRuntime2D>& bodies,
        std::vector<ContactConstraint2D>& contacts,
        const std::vector<PhysicsIsland2D>& islands)
    {
        auto makePairKey = [](PackedEntityId a, PackedEntityId b, uint32_t featureId) {
            if (a > b) std::swap(a, b);
            return PairKey{ a, b, featureId };
            };

        // Stage hot body properties in contiguous SoA arrays for tight cache access in inner loops.
        std::vector<float> invMass(bodies.size(), 0.0f);
        std::vector<float> friction(bodies.size(), 0.0f);
        std::vector<float> restitution(bodies.size(), 0.0f);
        std::vector<float> positionPercent(bodies.size(), 0.0f);
        std::vector<Vector2D> linearVelocity(bodies.size(), Vector2D(0.0f, 0.0f));
        std::vector<Vector2D> positions(bodies.size(), Vector2D(0.0f, 0.0f));

        for (size_t i = 0; i < bodies.size(); ++i) {
            const BodyRuntime2D& b = bodies[i];
            invMass[i] = InvMassOf(b.Rigidbody);
            friction[i] = std::clamp(b.Material.Friction, 0.0f, 1.0f);
            restitution[i] = std::clamp(b.Material.Restitution, 0.0f, 1.0f);
            positionPercent[i] = std::clamp(b.Material.PositionCorrectPercent, 0.0f, 1.0f);
            linearVelocity[i] = b.Velocity ? b.Velocity->Value : Vector2D(0.0f, 0.0f);
            positions[i] = b.Local ? Vector2D(b.Local->Position.X, b.Local->Position.Y) : Vector2D(0.0f, 0.0f);
        }

        for (ContactConstraint2D& c : contacts) {
            if (c.IsTrigger) {
                c.NormalImpulse = 0.0f;
                c.TangentImpulse = 0.0f;
                continue;
            }
            // Restore cached impulses for warm-start if we can match the same feature pair.
            const PairKey key = makePairKey(c.PackedA, c.PackedB, c.FeatureId);
            const auto it = m_warmStartCache.find(key);
            if (it != m_warmStartCache.end()) {
                // Reuse last frame impulses to reduce solver jitter on persistent contacts.
                c.NormalImpulse = std::max(it->second.NormalImpulse, 0.0f);
                c.TangentImpulse = it->second.TangentImpulse;
            }
            else {
                c.NormalImpulse = 0.0f;
                c.TangentImpulse = 0.0f;
            }

            const float invMassA = invMass[c.BodyA];
            const float invMassB = invMass[c.BodyB];
            if (invMassA + invMassB <= 0.0f || c.NormalImpulse == 0.0f) {
                continue;
            }

            // Apply warm-start impulse before iterations so solver starts close to previous solution.
            Vector2D vA = linearVelocity[c.BodyA];
            Vector2D vB = linearVelocity[c.BodyB];
            const Vector2D n = c.Manifold.normal;
            const Vector2D tangent(-n.Y, n.X);
            const Vector2D warmImpulse = n * c.NormalImpulse + tangent * c.TangentImpulse;
            vA -= warmImpulse * invMassA;
            vB += warmImpulse * invMassB;
            linearVelocity[c.BodyA] = vA;
            linearVelocity[c.BodyB] = vB;
        }

        // Stable ordering removes scheduler-dependent solve permutations.
        std::vector<size_t> islandOrder(islands.size(), 0);
        std::iota(islandOrder.begin(), islandOrder.end(), 0);
        std::sort(islandOrder.begin(), islandOrder.end(), [&islands, &bodies](size_t lhs, size_t rhs) {
            const auto& il = islands[lhs];
            const auto& ir = islands[rhs];
            const PackedEntityId pl = il.Bodies.empty() ? 0ull : bodies[il.Bodies.front()].Packed;
            const PackedEntityId pr = ir.Bodies.empty() ? 0ull : bodies[ir.Bodies.front()].Packed;
            if (pl != pr) return pl < pr;
            if (il.Bodies.size() != ir.Bodies.size()) return il.Bodies.size() < ir.Bodies.size();
            return lhs < rhs;
            });

#if !defined(NDEBUG)
        std::vector<PhysicsIsland2D> orderedIslands;
        orderedIslands.reserve(islandOrder.size());
        for (size_t i : islandOrder) {
            orderedIslands.push_back(islands[i]);
        }
        ValidateIslandOwnership(bodies.size(), contacts.size(), orderedIslands);
#endif

        constexpr uint32_t workerCap = 8;
        Internal::ParallelForStatic(islandOrder.size(), workerCap, [&config, &bodies, &contacts, &islandOrder, &islands, &invMass, &friction, &restitution, &positionPercent, &linearVelocity, &positions](size_t begin, size_t end, uint32_t) {
            for (size_t i = begin; i < end; ++i) {
                const PhysicsIsland2D& island = islands[islandOrder[i]];
                // Deterministic static scheduling + disjoint islands avoids cross-thread body writes.
                SolveIslandVelocity(config, contacts, island, invMass, friction, restitution, linearVelocity);
                SolveIslandPosition(config, bodies, contacts, island, invMass, positionPercent, positions);
            }
            });

        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].Velocity) {
                bodies[i].Velocity->Value = linearVelocity[i];
            }
            if (bodies[i].Local) {
                bodies[i].Local->Position.X = positions[i].X;
                bodies[i].Local->Position.Y = positions[i].Y;
            }
        }

        std::unordered_map<PairKey, WarmImpulseCacheEntry, PairKeyHash> nextCache;
        nextCache.reserve(contacts.size());
        for (const auto& c : contacts) {
            if (c.IsTrigger) {
                continue;
            }
            const PairKey key = makePairKey(c.PackedA, c.PackedB, c.FeatureId);
            // Persist final impulses for the next frame's warm-start phase.
            nextCache[key] = WarmImpulseCacheEntry{ c.NormalImpulse, c.TangentImpulse };
        }
        m_warmStartCache = std::move(nextCache);
    }
}
