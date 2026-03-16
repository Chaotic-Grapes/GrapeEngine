/* Start Header *****************************************************************/
/*!
\file   Narrowphase2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Narrowphase manifold generation for supported collider pairs.
\references
- https://dyn4j.org/2010/01/sat/
- https://box2d.org/files/ErinCatto_ContactManifolds_GDC2007.pdf
- https://box2d.org/documentation/
- https://realtimecollisiondetection.net/books/rtcd/
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
#include <cstdint>

namespace {
    /**
     * @brief Compact feature IDs used for manifold persistence keys.
     */
    enum : uint8_t {
        kFeatureNone = 0,
        kFeatureLeftFace = 1,
        kFeatureRightFace = 2,
        kFeatureBottomFace = 3,
        kFeatureTopFace = 4,
        kFeatureCornerBL = 5,
        kFeatureCornerBR = 6,
        kFeatureCornerTR = 7,
        kFeatureCornerTL = 8,
        kFeatureCircle = 9
    };

    /**
     * @brief Convert an axis/sign pair into a canonical box face feature ID.
     * @param axisX `true` for X-axis faces, `false` for Y-axis faces.
     * @param positive `true` for +axis face, `false` for -axis face.
     * @return Feature ID for the chosen face.
     */
    static uint8_t FaceFromAxis(bool axisX, bool positive) {
        if (axisX) {
            return positive ? kFeatureRightFace : kFeatureLeftFace;
        }
        return positive ? kFeatureTopFace : kFeatureBottomFace;
    }

    /**
     * @brief Pack manifold feature pair bytes into a deterministic 32-bit key.
     * @param pair Manifold feature-pair payload.
     * @return Packed feature key.
     */
    static uint32_t PackFeaturePair(const Engine::Collision::ContactManifold::FeaturePair& pair) {
        // Byte packing preserves exact feature identity across frames.
        return static_cast<uint32_t>(pair.inEdge1) |
            (static_cast<uint32_t>(pair.outEdge1) << 8) |
            (static_cast<uint32_t>(pair.inEdge2) << 16) |
            (static_cast<uint32_t>(pair.outEdge2) << 24);
    }

    /**
     * @brief Classify local-space point against a box boundary feature.
     * @param localPoint Contact point in box-local coordinates.
     * @param halfExtents Box half extents.
     * @return Face/corner feature ID.
     */
    static uint8_t ClassifyBoxFeatureLocal(const Vector2D& localPoint, const Vector2D& halfExtents) {
        constexpr float edgeEps = 1e-4f;
        const bool onLeft = std::abs(localPoint.X + halfExtents.X) <= edgeEps;
        const bool onRight = std::abs(localPoint.X - halfExtents.X) <= edgeEps;
        const bool onBottom = std::abs(localPoint.Y + halfExtents.Y) <= edgeEps;
        const bool onTop = std::abs(localPoint.Y - halfExtents.Y) <= edgeEps;

        if (onLeft && onBottom) return kFeatureCornerBL;
        if (onRight && onBottom) return kFeatureCornerBR;
        if (onRight && onTop) return kFeatureCornerTR;
        if (onLeft && onTop) return kFeatureCornerTL;
        if (onLeft) return kFeatureLeftFace;
        if (onRight) return kFeatureRightFace;
        if (onBottom) return kFeatureBottomFace;
        if (onTop) return kFeatureTopFace;

        const float dxLeft = std::abs(localPoint.X + halfExtents.X);
        const float dxRight = std::abs(localPoint.X - halfExtents.X);
        const float dyBottom = std::abs(localPoint.Y + halfExtents.Y);
        const float dyTop = std::abs(localPoint.Y - halfExtents.Y);
        // Fallback picks nearest face when contact point is numerically inside.
        if (std::min(dxLeft, dxRight) <= std::min(dyBottom, dyTop)) {
            return (dxLeft <= dxRight) ? kFeatureLeftFace : kFeatureRightFace;
        }
        return (dyBottom <= dyTop) ? kFeatureBottomFace : kFeatureTopFace;
    }

    /**
     * @brief 2D dot product helper.
     * @param a First vector.
     * @param b Second vector.
     * @return Dot product.
     */
    static float Dot2D(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    /**
     * @brief Circle-circle overlap test with normal/depth output.
     * @param a First world circle.
     * @param b Second world circle.
     * @param outNormal Collision normal from `a` to `b`.
     * @param outDepth Penetration depth.
     * @return `true` when circles overlap.
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
     * @param obbA First OBB.
     * @param obbB Second OBB.
     * @return Contact manifold; `pointCount == 0` means no overlap.
     */
    static Engine::Collision::ContactManifold TestBoxBox(const Engine::WorldOBB& obbA, const Engine::WorldOBB& obbB) {
        Engine::Collision::ContactManifold manifold;
        manifold.pointCount = 0;
        const Vector2D delta = obbB.Center - obbA.Center;
        const Vector2D axes[4] = { obbA.AxisX, obbA.AxisY, obbB.AxisX, obbB.AxisY };
        float minOverlap = FLT_MAX;
        Vector2D bestAxis{ 0.0f, 0.0f };
        int bestAxisIndex = 0;
        for (int i = 0; i < 4; ++i) {
            const Vector2D& axis = axes[i];
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
                bestAxisIndex = i;
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
        const bool axisX = (bestAxisIndex % 2) == 0;
        const bool positive = Dot2D(delta, axes[bestAxisIndex]) >= 0.0f;
        const uint8_t faceA = FaceFromAxis(axisX, positive);
        const uint8_t faceB = FaceFromAxis(axisX, !positive);
        manifold.ids[0].inEdge1 = faceA;
        manifold.ids[0].outEdge1 = faceA;
        manifold.ids[0].inEdge2 = faceB;
        manifold.ids[0].outEdge2 = faceB;
        return manifold;
    }

    /**
     * @brief Circle-vs-box overlap test supporting axis-aligned and rotated boxes.
     * @param circle Circle in world space.
     * @param box OBB in world space.
     * @param outNormal Collision normal from circle to box.
     * @param outDepth Penetration depth.
     * @param outContact Contact point in world space.
     * @param outBoxFeature Box feature ID at contact.
     * @return `true` when overlap exists.
     */
    static bool TestCircleBox(
        const Engine::WorldCircle& circle,
        const Engine::WorldOBB& box,
        Vector2D& outNormal,
        float& outDepth,
        Vector2D& outContact,
        uint8_t& outBoxFeature)
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
            outBoxFeature = ClassifyBoxFeatureLocal(outContact - box.Center, box.HalfExtents);
            return true;
        }

        const Vector2D toCircle = circle.Center - box.Center;
        // Rotate into box-local frame so circle-vs-AABB logic can be reused deterministically.
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
        outBoxFeature = ClassifyBoxFeatureLocal(localManifold.Contact, box.HalfExtents);
        return true;
    }
}

namespace Engine::Physics2D {
    /**
     * @brief Build manifold for a supported shape pair.
     * @param a First body.
     * @param b Second body.
     * @param out Output contact manifold.
     * @return `true` when the pair overlaps.
     */
    bool Narrowphase2D::BuildManifold(const BodyRuntime2D& a, const BodyRuntime2D& b, Engine::Collision::ContactManifold& out) {
        out = Engine::Collision::ContactManifold{};
        if (a.Shape == ShapeType2D::Circle && b.Shape == ShapeType2D::Circle) {
            // Circle-circle manifold: normal from A to B, single averaged point.
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
            out.ids[0].inEdge1 = kFeatureCircle;
            out.ids[0].outEdge1 = kFeatureCircle;
            out.ids[0].inEdge2 = kFeatureCircle;
            out.ids[0].outEdge2 = kFeatureCircle;
            return true;
        }
        if (a.Shape == ShapeType2D::Box && b.Shape == ShapeType2D::Box) {
            // Box-box manifold currently uses SAT + one support-point contact.
            out = TestBoxBox(a.WorldObb, b.WorldObb);
            return out.pointCount > 0;
        }
        if (a.Shape == ShapeType2D::Circle && b.Shape == ShapeType2D::Box) {
            Vector2D n{};
            float depth = 0.0f;
            Vector2D contact{};
            uint8_t boxFeature = kFeatureNone;
            if (!TestCircleBox(a.WorldCircle, b.WorldObb, n, depth, contact, boxFeature)) {
                return false;
            }
            out.normal = n;
            out.penetration = depth;
            out.points[0] = contact;
            out.pointCount = 1;
            out.ids[0].inEdge1 = kFeatureCircle;
            out.ids[0].outEdge1 = kFeatureCircle;
            out.ids[0].inEdge2 = boxFeature;
            out.ids[0].outEdge2 = boxFeature;
            return true;
        }
        if (a.Shape == ShapeType2D::Box && b.Shape == ShapeType2D::Circle) {
            Vector2D n{};
            float depth = 0.0f;
            Vector2D contact{};
            uint8_t boxFeature = kFeatureNone;
            if (!TestCircleBox(b.WorldCircle, a.WorldObb, n, depth, contact, boxFeature)) {
                return false;
            }
            // Flip normal so manifold convention remains from A to B.
            out.normal = -n;
            out.penetration = depth;
            out.points[0] = contact;
            out.pointCount = 1;
            out.ids[0].inEdge1 = boxFeature;
            out.ids[0].outEdge1 = boxFeature;
            out.ids[0].inEdge2 = kFeatureCircle;
            out.ids[0].outEdge2 = kFeatureCircle;
            return true;
        }
        return false;
    }

    /**
     * @brief Build contact constraints from candidate broadphase pairs.
     * @param bodies Runtime body array.
     * @param pairs Broadphase candidate pairs.
     * @return Sorted contact constraints for solver/events.
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
                // Convert geometric manifold into solver/event-friendly contact constraint.
                ContactConstraint2D c{};
                c.BodyA = pair.BodyA;
                c.BodyB = pair.BodyB;
                c.PackedA = pair.PackedA;
                c.PackedB = pair.PackedB;
                c.TriggerA = a.IsTrigger;
                c.TriggerB = b.IsTrigger;
                c.IsTrigger = c.TriggerA || c.TriggerB;
                c.Manifold = manifold;
                c.FeatureId = PackFeaturePair(manifold.ids[0]);
                localContacts.push_back(c);
            }
            });
        std::vector<ContactConstraint2D> contacts;
        for (auto& local : perWorker) {
            // Concatenate per-worker outputs in fixed worker order for determinism.
            contacts.insert(contacts.end(), local.begin(), local.end());
        }
        std::sort(contacts.begin(), contacts.end(), [](const ContactConstraint2D& lhs, const ContactConstraint2D& rhs) {
            if (lhs.PackedA != rhs.PackedA) return lhs.PackedA < rhs.PackedA;
            return lhs.PackedB < rhs.PackedB;
            });
        return contacts;
    }
}
