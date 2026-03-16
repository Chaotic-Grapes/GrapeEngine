/* Start Header *****************************************************************/
/*!
\file   Narrowphase2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Narrowphase manifold generation for supported collider pairs.
*/
/* End Header *******************************************************************/

#include "physics2d/Narrowphase2D.h"
#include "physics2d/internal/ParallelFor.h"
#include "physics2d/internal/SimdKernels2D.h"
#include "physics/Physics.h"
#include "physics/LayerMask.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    /**
     * @brief 2D dot product helper.
     */
    static float Dot2D(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    /**
     * @brief Circle-circle overlap test with normal/depth output.
     */
    static bool TestCircleCircle(
        const Engine::WorldCircle& a,
        const Engine::WorldCircle& b,
        Vector2D& outNormal,
        float& outDepth)
    {
        return Engine::Physics2D::Internal::CircleCircle(a, b, outNormal, outDepth);
    }

    /**
     * @brief Build a simple single-point manifold for OBB-vs-OBB overlap.
     */
    static Engine::Collision::ContactManifold TestBoxBox(const Engine::WorldOBB& obbA, const Engine::WorldOBB& obbB) {
        Engine::Collision::ContactManifold manifold;
        manifold.pointCount = 0;
        const Vector2D delta = obbB.Center - obbA.Center;
        const Vector2D axes[4] = { obbA.AxisX, obbA.AxisY, obbB.AxisX, obbB.AxisY };
        float minOverlap = FLT_MAX;
        Vector2D bestAxis{ 0.0f, 0.0f };
        for (const auto& axis : axes) {
            // SAT: project both OBBs onto each candidate separating axis.
            const float rA = obbA.HalfExtents.X * std::abs(Dot2D(axis, obbA.AxisX)) +
                obbA.HalfExtents.Y * std::abs(Dot2D(axis, obbA.AxisY));
            const float rB = obbB.HalfExtents.X * std::abs(Dot2D(axis, obbB.AxisX)) +
                obbB.HalfExtents.Y * std::abs(Dot2D(axis, obbB.AxisY));
            const float dist = std::abs(Dot2D(delta, axis));
            const float overlap = rA + rB - dist;
            if (overlap <= 0.0f) {
                return manifold;
            }
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = (Dot2D(delta, axis) < 0.0f) ? (axis * -1.0f) : axis;
            }
        }
        manifold.normal = bestAxis;
        manifold.penetration = minOverlap;
        const auto support = [](const Engine::WorldOBB& obb, const Vector2D& dir) {
            // Pick the furthest corner in `dir` by choosing signs along local axes.
            const float sx = (Dot2D(dir, obb.AxisX) >= 0.0f) ? 1.0f : -1.0f;
            const float sy = (Dot2D(dir, obb.AxisY) >= 0.0f) ? 1.0f : -1.0f;
            return obb.Center + obb.AxisX * (obb.HalfExtents.X * sx) + obb.AxisY * (obb.HalfExtents.Y * sy);
        };
        const Vector2D pA = support(obbA, manifold.normal);
        const Vector2D pB = support(obbB, manifold.normal * -1.0f);
        manifold.points[0] = (pA + pB) * 0.5f;
        manifold.pointCount = 1;
        return manifold;
    }

    /**
     * @brief Circle-vs-box overlap test supporting axis-aligned and rotated boxes.
     */
    static bool TestCircleBox(
        const Engine::WorldCircle& circle,
        const Engine::WorldOBB& box,
        Vector2D& outNormal,
        float& outDepth,
        Vector2D& outContact)
    {
        if (std::abs(box.Rotation) < 1e-6f) {
            Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(box.Center, box.HalfExtents * 2.0f);
            Engine::Collision::Circle circ{ circle.Center, circle.Radius };
            Engine::Collision::Manifold manifold;
            if (!Engine::Collision::Overlap(circ, aabb, &manifold)) {
                return false;
            }
            outNormal = manifold.Normal;
            outDepth = manifold.Penetration;
            outContact = manifold.Contact;
            return true;
        }

        const Vector2D toCircle = circle.Center - box.Center;
        // Rotate circle center into OBB local frame, solve as circle-vs-AABB, then rotate result back.
        const Vector2D localCenter{ Dot2D(toCircle, box.AxisX), Dot2D(toCircle, box.AxisY) };
        Engine::Collision::Circle localCircle{ localCenter, circle.Radius };
        const Engine::Collision::AABB localBox = Engine::Collision::MakeAABBCenterSize(
            Vector2D{ 0.0f, 0.0f }, Vector2D{ box.HalfExtents.X * 2.0f, box.HalfExtents.Y * 2.0f });
        Engine::Collision::Manifold localManifold;
        if (!Engine::Collision::Overlap(localCircle, localBox, &localManifold)) {
            return false;
        }

        outNormal = box.AxisX * localManifold.Normal.X + box.AxisY * localManifold.Normal.Y;
        outDepth = localManifold.Penetration;
        outContact = box.Center + box.AxisX * localManifold.Contact.X + box.AxisY * localManifold.Contact.Y;
        return true;
    }
}

namespace Engine::Physics2D {
    /**
     * @brief Build manifold for a supported shape pair.
     */
    bool Narrowphase2D::BuildManifold(const BodyRuntime2D& a, const BodyRuntime2D& b, Engine::Collision::ContactManifold& out) {
        out = Engine::Collision::ContactManifold{};
        if (a.Shape == ShapeType2D::Circle && b.Shape == ShapeType2D::Circle) {
            Vector2D n{};
            float depth = 0.0f;
            if (!TestCircleCircle(a.WorldCircle, b.WorldCircle, n, depth)) {
                return false;
            }
            out.normal = n;
            out.penetration = depth;
            const Vector2D pA = a.WorldCircle.Center + (n * a.WorldCircle.Radius);
            const Vector2D pB = b.WorldCircle.Center - (n * b.WorldCircle.Radius);
            out.points[0] = (pA + pB) * 0.5f;
            out.pointCount = 1;
            return true;
        }
        if (a.Shape == ShapeType2D::Box && b.Shape == ShapeType2D::Box) {
            out = TestBoxBox(a.WorldObb, b.WorldObb);
            return out.pointCount > 0;
        }
        if (a.Shape == ShapeType2D::Circle && b.Shape == ShapeType2D::Box) {
            Vector2D n{};
            float depth = 0.0f;
            Vector2D contact{};
            if (!TestCircleBox(a.WorldCircle, b.WorldObb, n, depth, contact)) {
                return false;
            }
            out.normal = n;
            out.penetration = depth;
            out.points[0] = contact;
            out.pointCount = 1;
            return true;
        }
        if (a.Shape == ShapeType2D::Box && b.Shape == ShapeType2D::Circle) {
            Vector2D n{};
            float depth = 0.0f;
            Vector2D contact{};
            if (!TestCircleBox(b.WorldCircle, a.WorldObb, n, depth, contact)) {
                return false;
            }
            out.normal = -n;
            out.penetration = depth;
            out.points[0] = contact;
            out.pointCount = 1;
            return true;
        }
        return false;
    }

    /**
     * @brief Build contact constraints from candidate broadphase pairs.
     */
    std::vector<ContactConstraint2D> Narrowphase2D::BuildContactsParallel(
        const std::vector<BodyRuntime2D>& bodies,
        const std::vector<BroadphasePair2D>& pairs) const
    {
        constexpr uint32_t workerCap = 8;
        std::vector<std::vector<ContactConstraint2D>> perWorker(workerCap);
        Internal::ParallelForStatic(pairs.size(), workerCap, [&bodies, &pairs, &perWorker](size_t begin, size_t end, uint32_t workerIdx) {
            std::vector<ContactConstraint2D>& localContacts = perWorker[workerIdx];
            localContacts.reserve(localContacts.size() + (end - begin));
            for (size_t i = begin; i < end; ++i) {
                const auto& pair = pairs[i];
                const BodyRuntime2D& a = bodies[pair.BodyA];
                const BodyRuntime2D& b = bodies[pair.BodyB];
                if (!Engine::CanCollide(a.LayerMask, a.LayerId, b.LayerMask, b.LayerId)) {
                    // Layer filtering is applied before expensive manifold generation.
                    continue;
                }
                Engine::Collision::ContactManifold manifold;
                if (!Narrowphase2D::BuildManifold(a, b, manifold)) {
                    continue;
                }
                ContactConstraint2D c{};
                c.BodyA = pair.BodyA;
                c.BodyB = pair.BodyB;
                c.PackedA = pair.PackedA;
                c.PackedB = pair.PackedB;
                c.TriggerA = a.IsTrigger;
                c.TriggerB = b.IsTrigger;
                c.IsTrigger = c.TriggerA || c.TriggerB;
                c.Manifold = manifold;
                localContacts.push_back(c);
            }
            });
        std::vector<ContactConstraint2D> contacts;
        for (auto& local : perWorker) {
            contacts.insert(contacts.end(), local.begin(), local.end());
        }
        std::sort(contacts.begin(), contacts.end(), [](const ContactConstraint2D& lhs, const ContactConstraint2D& rhs) {
            if (lhs.PackedA != rhs.PackedA) return lhs.PackedA < rhs.PackedA;
            return lhs.PackedB < rhs.PackedB;
            });
        return contacts;
    }
}
