/* Start Header *****************************************************************/
/*!
\file   Solver2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Sequential impulse solver implementation for contact islands.
*/
/* End Header *******************************************************************/

#include "physics2d/Solver2D.h"
#include "physics2d/internal/ParallelFor.h"
#include "physics/Physics.h"
#include <algorithm>

namespace Engine::Physics2D {
    /**
     * @brief Solve one connected island for configured velocity iterations.
     */
    void Solver2D::SolveIsland(
        const PhysicsConfig2D& config,
        std::vector<BodyRuntime2D>& bodies,
        const std::vector<ContactConstraint2D>& contacts,
        const PhysicsIsland2D& island)
    {
        for (uint32_t it = 0; it < config.VelocityIterations; ++it) {
            for (size_t cidx : island.Contacts) {
                if (cidx >= contacts.size()) {
                    continue;
                }
                const ContactConstraint2D& c = contacts[cidx];
                if (c.IsTrigger) {
                    continue;
                }

                BodyRuntime2D& a = bodies[c.BodyA];
                BodyRuntime2D& b = bodies[c.BodyB];
                if (!a.Local || !b.Local) {
                    continue;
                }

                ECS::Components::Rigidbody2D rbA{};
                ECS::Components::Rigidbody2D rbB{};
                if (a.Rigidbody) {
                    rbA = *a.Rigidbody;
                } else {
                    // Missing rigidbody is treated as static body for impulse solve.
                    rbA.Mass = 0.0f;
                    rbA.InverseMass = 0.0f;
                }
                if (b.Rigidbody) {
                    rbB = *b.Rigidbody;
                } else {
                    // Missing rigidbody is treated as static body for impulse solve.
                    rbB.Mass = 0.0f;
                    rbB.InverseMass = 0.0f;
                }

                ECS::Components::LinearVelocity2D vA{};
                ECS::Components::LinearVelocity2D vB{};
                if (a.Velocity) {
                    vA = *a.Velocity;
                }
                if (b.Velocity) {
                    vB = *b.Velocity;
                }
                const ECS::Components::PhysicsMaterial2D mat{
                    (a.Material.Friction + b.Material.Friction) * 0.5f,
                    std::max(a.Material.Restitution, b.Material.Restitution),
                    (a.Material.PositionCorrectPercent + b.Material.PositionCorrectPercent) * 0.5f
                };

                Engine::Physics::ResolveCollisionManifold(rbA, rbB, vA, vB, *a.Local, *b.Local, c.Manifold, mat);
                if (a.Velocity) {
                    *a.Velocity = vA;
                }
                if (b.Velocity) {
                    *b.Velocity = vB;
                }
            }
        }
    }

    /**
     * @brief Dispatch islands with deterministic static partitioning.
     */
    void Solver2D::Solve(
        const PhysicsConfig2D& config,
        std::vector<BodyRuntime2D>& bodies,
        const std::vector<ContactConstraint2D>& contacts,
        const std::vector<PhysicsIsland2D>& islands) const
    {
        // Deterministic island ownership: each worker gets fixed contiguous island ranges.
        constexpr uint32_t workerCap = 8;
        Internal::ParallelForStatic(islands.size(), workerCap, [&config, &bodies, &contacts, &islands](size_t begin, size_t end, uint32_t) {
            for (size_t i = begin; i < end; ++i) {
                SolveIsland(config, bodies, contacts, islands[i]);
            }
            });
    }
}
