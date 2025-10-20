/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu  
 * @file    Collision.cpp
 * @brief   Narrow-phase helpers for 2D collisions and simple sweep tests.
 *
 * @details Implements:
 *   � Line segment builder with outward unit normal (MakeSegment).
 *   � Robust quadratic solver used by sweep tests (solveQuadratic). 
 *   � Point-sweep hit against a circle (endpoint corner test). 
 *   � Moving circle vs static segment sweep, including optional endpoint checks.
 *   � Post-impact response: reflect remaining motion about contact normal. 
 *   � Point vs segment proximity test with tolerance. 
 *   � AABB vs AABB overlap test with separating normal & penetration. 
 *
 * @usage
 *   - Use CircleVsSegmentSweep() to detect the earliest TOI (time of impact) for a moving
 *     circle against a static segment; if it hits, call CircleSegmentResponse() to reflect
 *     your intended end position for simple sliding.
 *
 * @note
 *   - Coordinates assume a standard 2D screen space with X right, Y up (adjust as needed).
 *   - EPS and BIGF constants are local helpers for numerical stability.
 *
 * @dependencies
 *   - Math/Vector2D, Collision.h, <cmath>, <limits>, <algorithm>.
 *
 */


#include "physics/Collision.h"
#include <cmath>      // std::sqrt, std::fabs
#include <limits>     // std::numeric_limits
#include <algorithm>  // std::min
#include "Math/Vector2D.h"

namespace {
    constexpr float EPS = 1e-6f;
    constexpr float BIGF = std::numeric_limits<float>::max();
}

namespace Engine {
    // Builders
    Collision::LineSegment Collision::MakeSegment(const Vector2D& p0, const Vector2D& p1) {
        LineSegment s;
        s.P0 = p0;
        s.P1 = p1;

        const Vector2D edge = p1 - p0;

        // Legacy convention: outward normal from (edge.Y, -edge.X), then normalize
        const Vector2D n(edge.Y, -edge.X);
        const float l2 = n.SquareLength();
        s.Normal = (l2 > 0.0f) ? n * (1.0f / std::sqrt(l2)) : Vector2D(0.0f, 1.0f);
        return s;
    }

    Collision::AABB Collision::MakeAABBMinMax(const Vector2D& minPt, const Vector2D& maxPt) {
        return { minPt, maxPt };
    }
    Collision::AABB Collision::MakeAABBCenterSize(const Vector2D& center, const Vector2D& size) {
        const Vector2D half = Vector2D(size.X * 0.5f, size.Y * 0.5f);
        return { Vector2D(center.X - half.X, center.Y - half.Y),
                 Vector2D(center.X + half.X, center.Y + half.Y) };
    }

    Collision::Triangle Collision::MakeEquilateral(const Vector2D& c, const float side, const float angleRad) {
        const float h = (std::sqrt(3.0f) * 0.5f) * side;
        const float hb = 0.5f * side;
        const float hh = (2.0f / 3.0f) * h;  // centroid -> top
        const float lh = (1.0f / 3.0f) * h;  // centroid -> base line

        const Vector2D v0Local(0.0f, hh);
        const Vector2D v1Local(-hb, -lh);
        const Vector2D v2Local(hb, -lh);

        const float cs = std::cos(angleRad);
        const float sn = std::sin(angleRad);
        auto rot = [&](const Vector2D& p) { return Vector2D(p.X * cs - p.Y * sn, p.X * sn + p.Y * cs); };

        Triangle t;
        t.V0 = c + rot(v0Local);
        t.V1 = c + rot(v1Local);
        t.V2 = c + rot(v2Local);
        return t;
    }


    // Quadratic solver
    bool Collision::_solveQuadratic(const float a, const float b, const float c, float& t0, float& t1) {
        if (std::fabs(a) < EPS) {
            if (std::fabs(b) < EPS)
                return false;
            t0 = t1 = -c / b;
            return true;
        }
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f)
            return false;

        const float sqrtD = std::sqrt(disc);
        const float q = (b > 0.0f)
            ? -0.5f * (b + sqrtD)
            : -0.5f * (b - sqrtD);

        t0 = q / a;
        t1 = c / q;

        if (t0 > t1)
            std::swap(t0, t1);

        return true;
    }


    // Swept point (circle center) vs point (segment endpoint)
    bool Collision::_pointSweepHitCircle(const Vector2D& c0, const Vector2D& c1,
        const Vector2D& p, const float radius, float& tHit, Vector2D& normalAtHit)
    {
        const Vector2D v = c1 - c0;  // motion
        const Vector2D w = c0 - p;   // from endpoint to start center

        const float a = v.Dot(v);
        const float b = 2.0f * v.Dot(w);
        const float c = w.Dot(w) - radius * radius;

        float t0, t1;
        if (!_solveQuadratic(a, b, c, t0, t1))
            return false;

        // earliest valid root in [0,1]
        float tCandidate = BIGF;
        if (t0 >= 0.0f && t0 <= 1.0f) tCandidate = t0;
        if (t1 >= 0.0f && t1 <= 1.0f) tCandidate = std::min(tCandidate, t1);
        if (tCandidate == BIGF)
            return false;

        tHit = tCandidate;
        const Vector2D cVec = c0 + v * tHit;
        const Vector2D n = cVec - p; // outward from endpoint to circle center
        // float L2 = n.SquareLength();
        // normalAtHit = (L2 > 0.0f) ? n * (1.0f / std::sqrt(L2)) : Vector2D(0.0f, 1.0f);
        normalAtHit = n.Normalized();
        return true;
    }

    /* ================================================================
       Moving Circle vs Static Segment (swept)
       ================================================================ */
    bool Collision::CircleVsSegmentSweep(
        const Circle& circle,
        const Vector2D& intendedEnd,
        const LineSegment& segment,
        Vector2D& outContactPoint,
        Vector2D& outNormal,
        float& outTime,
        const bool checkEdges)
    {
        const Vector2D c0 = circle.Center;
        const Vector2D c1 = intendedEnd;
        const Vector2D v = c1 - c0;

        // Signed distances to line (plane) at start and end: n�(C - p0)
        const float d0 = segment.Normal.Dot(c0 - segment.P0);
        const float d1 = segment.Normal.Dot(c1 - segment.P0);

        // If both start and end are farther than radius on the outward side, no hit
        if (d0 > circle.Radius && d1 > circle.Radius) return false;

        // Motion parallel to plane?
        const float denom = segment.Normal.Dot(v);
        if (std::fabs(denom) > EPS) {
            // Solve for t where the *inflated* plane is touched: d(t) = r
            const float t = (circle.Radius - d0) / denom;
            if (t >= 0.0f && t <= 1.0f) {
                const Vector2D c = c0 + v * t;                         // center at impact
                const Vector2D q = c - segment.Normal * circle.Radius;     // contact point on the segment line

                // Check if Q projects inside segment extents
                const Vector2D ab = segment.P1 - segment.P0;
                const float abLen2 = ab.SquareLength();
                float s = 0.0f;
                if (abLen2 > 0.0f)
                    s = (q - segment.P0).Dot(ab) / abLen2;

                if (s >= 0.0f && s <= 1.0f) {
                    outContactPoint = q;
                    outNormal = segment.Normal;  // unit
                    outTime = t;

                    return true;
                }
            }
        }

        // If we get here, either moving parallel OR plane hit fell outside the segment.
        // Optionally test segment endpoints as circles of radius r (corner hits).
        if (checkEdges) {
            float   bestT = BIGF;
            Vector2D bestN(0.0f, 0.0f), bestQ(0.0f, 0.0f);
            float tHit; Vector2D nHit;

            if (_pointSweepHitCircle(c0, c1, segment.P0, circle.Radius, tHit, nHit)) {
                if (tHit < bestT) {
                    bestT = tHit;
                    bestN = nHit;
                    bestQ = c0 + v * tHit - nHit * circle.Radius;
                }
            }

            if (_pointSweepHitCircle(c0, c1, segment.P1, circle.Radius, tHit, nHit)) {
                if (tHit < bestT) {
                    bestT = tHit;
                    bestN = nHit;
                    bestQ = c0 + v * tHit - nHit * circle.Radius;
                }
            }

            if (bestT < BIGF || bestT > BIGF) {
                outContactPoint = bestQ;
                outNormal = bestN;
                outTime = bestT;
                return true;
            }
        }

        return false;
    }

    /* ================================================================
       Response: reflect remaining motion about the normal
       ================================================================ */
    void Collision::CircleSegmentResponse(
        const Vector2D& contactPoint,
        const Vector2D& normal,
        Vector2D& inOutIntendedEnd,
        Vector2D& outReflectedDirection)
    {
        const Vector2D leftOver = inOutIntendedEnd - contactPoint; // remaining motion
        const float    along = leftOver.Dot(normal);
        const Vector2D reflected = leftOver - normal * (2.0f * along);

        // Update intended end to be mirrored about the surface
        inOutIntendedEnd = contactPoint + reflected;

        // Unit reflected direction for caller to reuse with its own speed
        const float l2 = reflected.SquareLength();
        outReflectedDirection = (l2 > 0.0f)
            ? reflected * (1.0f / std::sqrt(l2))
            : Vector2D(0.0f, 1.0f);
    }

    /* ================================================================
       Point vs Segment (closest point + tolerance test)
       ================================================================ */
    bool Collision::PointVsSegment(
        const Vector2D& p,
        const Vector2D& s0,
        const Vector2D& s1,
        const float tolerance,
        float* outTime,
        Vector2D* outClosest)
    {
        const Vector2D v = s1 - s0;
        const float len2 = v.SquareLength();

        float t = 0.0f;
        if (len2 > 0.0f) {
            t = (p - s0).Dot(v) / len2;
        }

        // clamp to [0,1] without relying on glm
        const float tc = Vector2D::ClampValue(t, 0.0f, 1.0f);
        const Vector2D q = s0 + v * tc;

        const bool hit = (p - q).SquareLength() <= (tolerance * tolerance);

        if (outTime)       *outTime = tc;
        if (outClosest) *outClosest = q;
        return hit;
    }

    /* ================================================================
       AABB vs AABB
       ================================================================ */
    bool Collision::AABBvsAABB(
        const AABB& a,
        const AABB& b,
        Vector2D* outNormal,
        float* outPenetration)
    {
        // Centers and half extents
        const Vector2D cA = (a.Min + a.Max) * 0.5f;
        const Vector2D cB = (b.Min + b.Max) * 0.5f;
        const Vector2D hA = (a.Max - a.Min) * 0.5f;
        const Vector2D hB = (b.Max - b.Min) * 0.5f;

        const float dx = cB.X - cA.X;
        const float px = (hA.X + hB.X) - std::fabs(dx);
        if (px <= 0.0f)
            return false; // no overlap X

        const float dy = cB.Y - cA.Y;
        const float py = (hA.Y + hB.Y) - std::fabs(dy);
        if (py <= 0.0f)
            return false; // no overlap Y

        Vector2D n(0.0f, 0.0f);
        float pen = 0.0f;

        if (px < py) {
            n.X = (dx < 0.0f) ? -1.0f : 1.0f;
            pen = px;
        }
        else {
            n.Y = (dy < 0.0f) ? -1.0f : 1.0f;
            pen = py;
        }

        if (outNormal)      *outNormal = n;
        if (outPenetration) *outPenetration = pen;
        return true;
    }

    /* ================================================================
       Overlap: Triangle vs AABB
       ================================================================ */
    bool Collision::Overlap(const Triangle& tri, const AABB& box) {
        const Vector2D triVerts[3] = { tri.V0, tri.V1, tri.V2 };
        const Vector2D triEdges[3] = { tri.V1 - tri.V0, tri.V2 - tri.V1, tri.V0 - tri.V2 };

        const Vector2D axes[5] = {
            Vector2D(-triEdges[0].Y, triEdges[0].X), // Perpendicular to edge 0
            Vector2D(-triEdges[1].Y, triEdges[1].X), // Perpendicular to edge 1
            Vector2D(-triEdges[2].Y, triEdges[2].X), // Perpendicular to edge 2
            Vector2D(1.0f, 0.0f),                   // AABB X-axis
            Vector2D(0.0f, 1.0f)                    // AABB Y-axis
        };

        const Vector2D boxVerts[4] = {
            box.Min,
            Vector2D(box.Min.X, box.Max.Y),
            box.Max,
            Vector2D(box.Max.X, box.Min.Y)
        };

        for (const auto& axis : axes) {
            float triMin = BIGF, triMax = -BIGF;
            for (const auto& vert : triVerts) {
                float proj = axis.Dot(vert);
                triMin = std::min(triMin, proj);
                triMax = std::max(triMax, proj);
            }

            float boxMin = BIGF, boxMax = -BIGF;
            for (const auto& vert : boxVerts) {
                float proj = axis.Dot(vert);
                boxMin = std::min(boxMin, proj);
                boxMax = std::max(boxMax, proj);
            }

            if (triMax < boxMin || boxMax < triMin) {
                return false; // Separating axis found
            }
        }

        return true; // No separating axis found
    }

    /* ================================================================
       Overlap: AABB vs AABB
       ================================================================ */
    bool Collision::Overlap(const AABB& a, const AABB& b, Manifold* m) {
        const float dx1 = b.Min.X - a.Max.X;
        const float dx2 = a.Min.X - b.Max.X;
        const float dy1 = b.Min.Y - a.Max.Y;
        const float dy2 = a.Min.Y - b.Max.Y;

        if (dx1 > 0.f || dx2 > 0.f || dy1 > 0.f || dy2 > 0.f) return false;
        if (!m) return true;

        const float px = std::min(a.Max.X - b.Min.X, b.Max.X - a.Min.X);
        const float py = std::min(a.Max.Y - b.Min.Y, b.Max.Y - a.Min.Y);

        if (px < py) {
            m->Normal = { (a.Max.X + a.Min.X < b.Max.X + b.Min.X) ? -1.f : 1.f, 0.f };
            m->Penetration = px;
        }
        else {
            m->Normal = { 0.f, (a.Max.Y + a.Min.Y < b.Max.Y + b.Min.Y) ? -1.f : 1.f };
            m->Penetration = py;
        }

        m->Valid = true;
        return true;
    }

    /* ================================================================
       Overlap: Circle vs Circle
       ================================================================ */
    bool Collision::Overlap(const Circle& a, const Circle& b, Manifold* m) {
        const Vector2D d = b.Center - a.Center;
        const float r = a.Radius + b.Radius;
        const float d2 = d.SquareLength();

        if (d2 > r * r) return false;
        if (!m) return true;

        const float dLen = std::sqrt(std::max(d2, EPS));
        m->Normal = (dLen > 0.f) ? d / dLen : Vector2D(0.f, 1.f);
        m->Penetration = r - dLen;
        m->Contact = b.Center - m->Normal * b.Radius;
        m->Valid = true;

        return true;
    }

    /* ================================================================
       Overlap: Circle vs AABB
       ================================================================ */
    bool Collision::Overlap(const Circle& a, const AABB& b, Manifold* m) {
        const float cx = std::min(std::max(a.Center.X, b.Min.X), b.Max.X);
        const float cy = std::min(std::max(a.Center.Y, b.Min.Y), b.Max.Y);
        const Vector2D closest(cx, cy);

        const Vector2D d = a.Center - closest;
        const float d2 = d.SquareLength();

        if (d2 > a.Radius * a.Radius) return false;
        if (!m) return true;

        const float dLen = std::sqrt(std::max(d2, EPS));
        m->Normal = (dLen > 0.f) ? d / dLen : Vector2D(0.f, 1.f);
        m->Penetration = a.Radius - dLen;
        m->Contact = closest;
        m->Valid = true;

        return true;
    }

    /* ================================================================
       Overlap: ConvexPolygon vs ConvexPolygon
       ================================================================ */
    bool Collision::Overlap(const ConvexPolygon& a, const ConvexPolygon& b, Manifold* m) {
        // Implementation omitted for brevity
        return false;
    }

    /* ================================================================
       Sweep: AABB vs AABB
       ================================================================ */
    Collision::SweepHit Collision::Sweep(const AABB& a, const Vector2D& aEnd,
        const AABB& b, const Vector2D& bEnd) {
        // Implementation omitted for brevity
        return {};
    }

    /* ================================================================
       Sweep: Circle vs Circle
       ================================================================ */
    Collision::SweepHit Collision::Sweep(const Circle& a, const Vector2D& aEnd,
        const Circle& b, const Vector2D& bEnd) {
        // Implementation omitted for brevity
        return {};
    }

    /* ================================================================
       Sweep: Circle vs AABB
       ================================================================ */
    Collision::SweepHit Collision::Sweep(const Circle& a, const Vector2D& aEnd,
        const AABB& b, const Vector2D& bEnd) {
        // Implementation omitted for brevity
        return {};
    }
}
