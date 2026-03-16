/* Start Header *****************************************************************/
/*!
\file   PhysicsQueries2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Runtime physics query utilities (raycast, overlap, point query).
\references
- https://tavianator.com/2011/ray_box.html
- https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection.html
- https://box2d.org/documentation/
- https://realtimecollisiondetection.net/books/rtcd/
*/
/* End Header *******************************************************************/

#include "physics2d/PhysicsQueries2D.h"
#include "physics/Collision.h"
#include <algorithm>

namespace Engine::Physics2D {
    /**
     * @brief Ray cast against cached body AABBs.
     */
    RayCastHit2D PhysicsQueries2D::RayCast(const std::vector<BodyRuntime2D>& bodies, const RayCastInput2D& input) {
        RayCastHit2D best{};
        best.Distance = input.MaxDistance;
        // Normalize once so all distance values are measured along same direction vector.
        const Vector2D dir = input.Direction.Normalized();

        for (const auto& b : bodies) {
            if (b.Shape == ShapeType2D::None) {
                continue;
            }

            // Query currently uses world AABB proxy (fast, conservative).
            Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(
                b.WorldAabb.Center, b.WorldAabb.HalfExtents * 2.0f);
            const Vector2D invDir(
                // Large sentinel keeps slab math finite for near-axis-aligned rays.
                std::abs(dir.X) < 1e-6f ? 1e30f : (1.0f / dir.X),
                std::abs(dir.Y) < 1e-6f ? 1e30f : (1.0f / dir.Y));
            const Vector2D mn = aabb.Min;
            const Vector2D mx = aabb.Max;
            const float tx1 = (mn.X - input.Origin.X) * invDir.X;
            const float tx2 = (mx.X - input.Origin.X) * invDir.X;
            const float ty1 = (mn.Y - input.Origin.Y) * invDir.Y;
            const float ty2 = (mx.Y - input.Origin.Y) * invDir.Y;
            const float tmin = std::max(std::min(tx1, tx2), std::min(ty1, ty2));
            const float tmax = std::min(std::max(tx1, tx2), std::max(ty1, ty2));
            // Reject misses, behind-origin intersections, and hits farther than current best.
            if (tmax < 0.0f || tmin > tmax || tmin < 0.0f || tmin > best.Distance) {
                continue;
            }

            // Keep nearest hit only.
            best.Hit = true;
            best.Entity = b.Entity;
            best.Distance = tmin;
            best.Point = input.Origin + dir * tmin;
            const Vector2D local = best.Point - b.WorldAabb.Center;
            if (std::abs(local.X) > std::abs(local.Y)) {
                // Approximate normal from dominant AABB axis at the hit point.
                best.Normal = Vector2D((local.X > 0.0f) ? 1.0f : -1.0f, 0.0f);
            }
            else {
                best.Normal = Vector2D(0.0f, (local.Y > 0.0f) ? 1.0f : -1.0f);
            }
        }
        return best;
    }

    /**
     * @brief Return entities whose AABBs overlap the query box.
     */
    std::vector<ECS::Entity> PhysicsQueries2D::OverlapAABB(const std::vector<BodyRuntime2D>& bodies, const Engine::WorldAABB& query) {
        std::vector<ECS::Entity> out;
        for (const auto& b : bodies) {
            if (b.Shape == ShapeType2D::None) {
                continue;
            }
            const Vector2D d = b.WorldAabb.Center - query.Center;
            // Axis-aligned overlap test on both X and Y intervals.
            const bool overlap = std::abs(d.X) <= (b.WorldAabb.HalfExtents.X + query.HalfExtents.X) &&
                std::abs(d.Y) <= (b.WorldAabb.HalfExtents.Y + query.HalfExtents.Y);
            if (overlap) {
                out.push_back(b.Entity);
            }
        }
        return out;
    }

    /**
     * @brief Return entities whose AABBs contain the query point.
     */
    std::vector<ECS::Entity> PhysicsQueries2D::QueryPoint(const std::vector<BodyRuntime2D>& bodies, const Vector2D& point) {
        std::vector<ECS::Entity> out;
        for (const auto& b : bodies) {
            if (b.Shape == ShapeType2D::None) {
                continue;
            }
            const Vector2D d = point - b.WorldAabb.Center;
            // Point-in-AABB test in local offset form.
            if (std::abs(d.X) <= b.WorldAabb.HalfExtents.X && std::abs(d.Y) <= b.WorldAabb.HalfExtents.Y) {
                out.push_back(b.Entity);
            }
        }
        return out;
    }
}
