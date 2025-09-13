
#pragma once
/*
  Collision.hpp
  ----------------------------------------------------------
  Single entry-point for 2D collision in your engine.

  - Coordinate system: orthographic pixel space (+X right, +Y up)
  - All math via GLM
  - Public nested types: Circle, LineSegment, AABB
  - Static API only: no instances required

  Shapes you can render are the shapes you can collide:
    * LineSegment (p0 -> p1) with outward unit normal (built from edge.y, -edge.x)
    * Circle       (center, radius)
    * AABB         (min, max) or build from (center, size)

  Typical frame usage:
    1) Integrate dynamic bodies: posNext = pos + vel * dt
    2) For each circle vs each wall segment:
         if dot(vel, wall.normal) <= 0:
           if Collision::CircleVsSegmentSweep(...):
             Collision::CircleSegmentResponse(...)
             vel = normalize(reflectedDir) * speed;
    3) For AABBs: if AABBvsAABB(...): push out by normal * penetration; zero vel along normal
    4) Commit positions; render
*/

#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <cmath>
#include <limits>

class Collision {
public:
    // --------------------------------------------------------
    // Public nested types
    // --------------------------------------------------------
    struct LineSegment {
        glm::vec2 p0{ 0.0f };
        glm::vec2 p1{ 0.0f };
        glm::vec2 normal{ 0.0f };  // unit outward normal
    };

    struct Circle {
        glm::vec2 center{ 0.0f };
        float     radius{ 0.0f };
    };

    struct AABB {
        glm::vec2 min{ 0.0f }; // bottom-left
        glm::vec2 max{ 0.0f }; // top-right
    };

    // --------------------------------------------------------
    // Builders
    // --------------------------------------------------------

    /*!
     * @brief Build a line segment with an outward unit normal.
     * @details Normal = normalize( vec2(edge.y, -edge.x) ). Flip if you need the other side.
     */
    static LineSegment MakeSegment(const glm::vec2& p0, const glm::vec2& p1);

    /*!
     * @brief Build an AABB directly from min/max.
     */
    static AABB MakeAABB_MinMax(const glm::vec2& minPt, const glm::vec2& maxPt) {
        return { minPt, maxPt };
    }

    /*!
     * @brief Build an AABB from center and size (pixels).
     *        min = center - size/2, max = center + size/2
     */
    static AABB MakeAABB_CenterSize(const glm::vec2& center, const glm::vec2& size) {
        glm::vec2 half = 0.5f * size;
        return { center - half, center + half };
    }

    // --------------------------------------------------------
    // Queries / Intersections
    // --------------------------------------------------------

    /*!
     * @brief Moving-circle vs static line segment (swept test).
     * @param circle   Circle at start of frame.
     * @param intendedEnd  Intended circle center after integration: center + vel*dt.
     * @param seg      Static segment with unit outward normal.
     * @param outContactPoint [out] Contact point on the segment (world space).
     * @param outNormal       [out] Contact normal (unit).
     * @param outTime         [out] t in [0,1] along the motion where contact occurs.
     * @param checkEdges      If true, falls back to testing segment endpoints (corner hits).
     * @return true if collision happens for some t in [0,1].
     *
     * @note For best performance, gate with dot(vel, seg.normal) <= 0 before calling.
     */
    static bool CircleVsSegmentSweep(
        const Circle& circle,
        const glm::vec2& intendedEnd,
        const LineSegment& seg,
        glm::vec2& outContactPoint,
        glm::vec2& outNormal,
        float& outTime,
        bool checkEdges = true);

    /*!
     * @brief Reflect the remaining motion about a surface normal.
     * @param contactPoint     World-space contact point.
     * @param normal           Unit surface normal.
     * @param inOutIntendedEnd [in/out] Intended end point; on return, mirrored about the surface.
     * @param outReflectedDir  [out] Reflected direction (unit).
     *
     * @details Uses v' = v - 2*dot(v,n)*n, where v is the leftover vector from contact->intendedEnd.
     */
    static void CircleSegmentResponse(
        const glm::vec2& contactPoint,
        const glm::vec2& normal,
        glm::vec2& inOutIntendedEnd,
        glm::vec2& outReflectedDir);

    /*!
     * @brief Closest-point query / tolerance hit for a point against a segment.
     * @param P         Point.
     * @param S0, S1    Segment endpoints.
     * @param tol       Tolerance (pixels). Hit if distance(P, segment) <= tol.
     * @param outT      [optional] Param in [0,1] of closest point on S0->S1.
     * @param outClosest[optional] Closest point.
     * @return true if within tolerance.
     */
    static bool PointVsSegment(
        const glm::vec2& P,
        const glm::vec2& S0,
        const glm::vec2& S1,
        float tol,
        float* outT = nullptr,
        glm::vec2* outClosest = nullptr);

    /*!
     * @brief AABB–AABB overlap + minimal manifold (normal, penetration).
     * @param A, B             Boxes.
     * @param outNormal        [optional] Separation normal pointing from A to outside B.
     * @param outPenetration   [optional] Depth along that normal.
     * @return true if overlap.
     */
    static bool AABBvsAABB(
        const AABB& A,
        const AABB& B,
        glm::vec2* outNormal = nullptr,
        float* outPenetration = nullptr);

private:
    // --------------------------------------------------------
    // Internal helpers
    // --------------------------------------------------------
    static bool solveQuadratic(float a, float b, float c, float& t0, float& t1);
    static bool pointSweepHitCircle(const glm::vec2& C0, const glm::vec2& C1,
        const glm::vec2& P, float radius,
        float& tHit, glm::vec2& normalAtHit);
};
