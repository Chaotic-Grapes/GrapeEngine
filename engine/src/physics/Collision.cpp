/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu  
 * @file    Collision.cpp
 * @brief   Narrow-phase helpers for 2D collisions and simple sweep tests.
 *
 * @details
 * This translation unit implements a small set of collision helper routines
 * used by the physics/engine layer:
 * - Primitive builders: LineSegment, AABB (min/max and center/size)
 * - Quadratic solver for continuous collision detection (CCD) helpers
 * - CCD: moving circle vs. static line segment (with optional edge checks)
 * - Response: reflect remaining motion about a surface normal
 * - Point vs. segment proximity test (with tolerance)
 * - Discrete overlap tests: AABB vs AABB, Circle vs Circle, Circle vs AABB
 * - SAT overlap: Triangle vs AABB
 *
 * The functions favor clarity and robustness (explicit clamping, epsilon
 * checks, and early outs) and are suitable for both gameplay queries and 
 * simple physics responses. All math is done in engine Vector2D.
 *
 * @references
 * https://realtimecollisiondetection.net/books/rtcd/
 * https://dyn4j.org/2010/01/sat/
 * https://tavianator.com/2011/ray_box.html
 * https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection.html
 * https://box2d.org/files/ErinCatto_ContactManifolds_GDC2007.pdf
 * https://box2d.org/documentation/
 *
 *
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "physics/Collision.h"
#include <cmath>      // std::sqrt, std::fabs
#include <limits>     // std::numeric_limits
#include <algorithm>  // std::min
#include "Math/Vector2D.h" // vector usage

namespace {
    constexpr float EPS = 1e-6f;
    constexpr float BIGF = std::numeric_limits<float>::max();

    inline float Dot(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }
}

namespace Engine {

    /* =====================================================================
    Helpers and primitive builders
    ===================================================================== */


    /**
    * @brief Build a line segment and compute its outward unit normal.
    */
    Collision::LineSegment Collision::MakeSegment(const Vector2D& p0, const Vector2D& p1) {
        LineSegment s;  // Output segment
        s.P0 = p0;      // Start point
        s.P1 = p1;      // End point

        const Vector2D edge = p1 - p0; // Edge vector from P0 to P1

        // Outward normal -> rotate edge by +90 degrees.
        const Vector2D n(edge.Y, -edge.X);
        const float l2 = n.SquareLength(); // Length squared of the unnormalized normal
        // Normalize if possible, otherwise pick a default unit up vector.
        s.Normal = (l2 > 0.0f) ? n * (1.0f / std::sqrt(l2)) : Vector2D(0.0f, 1.0f);
        return s;
    }

    /**
    * @brief Construct an AABB from center and size (width, height).
    */
    Collision::AABB Collision::MakeAABBCenterSize(const Vector2D& center, const Vector2D& size) {
        const Vector2D half = Vector2D(size.X * 0.5f, size.Y * 0.5f);// Half extents

        // Convert center/size to min/max.
        return { Vector2D(center.X - half.X, center.Y - half.Y),
                 Vector2D(center.X + half.X, center.Y + half.Y) };
    }


    /**
    * @brief Solve a t-quadratic: a t^2 + b t + c = 0. Returns sorted roots.
    * @return true if real roots exist, false otherwise.
    */
    bool Collision::_solveQuadratic(const float a, const float b, const float c, float& t0, float& t1) {
        // Handle near-linear case -> |a| ~ 0.
        if (std::fabs(a) < EPS) {
            // Degenerate further to constant if |b| ~ 0 -> then no solution.
            if (std::fabs(b) < EPS)
                return false;
            // Linear solution t = -c/b.
            t0 = t1 = -c / b;
            return true;
        }
        // Discriminant.
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f)
            return false;// No real roots.

        const float sqrtD = std::sqrt(disc); // Positive by definition

        // Stable form for computing the roots.
        const float q = (b > 0.0f)
            ? -0.5f * (b + sqrtD)
            : -0.5f * (b - sqrtD);

        t0 = q / a; // One root
        t1 = c / q; // The other

        if (t0 > t1) std::swap(t0, t1); // Ensure ascending order

        return true; // real root exists
    }

    /* =====================================================================
    Swept point (circle center) vs point (segment endpoint)
    ===================================================================== */


    /**
    * @brief Sweep a moving point (circle center) against a static point.
    * 
    * @return true if the swept center hits the point-expanded circle.
    *
    * This is used to handle corner hits when sweeping a circle against a
    * segment: you test each endpoint as a circle of radius R (Minkowski sum).
    */

    bool Collision::_pointSweepHitCircle(const Vector2D& c0, const Vector2D& c1,
        const Vector2D& p, const float radius, float& tHit, Vector2D& normalAtHit)
    {
        const Vector2D v = c1 - c0; // Motion vector of the center
        const Vector2D w = c0 - p; // Vector from endpoint to start center

        // Solve |w + v t|^2 = r^2 for t in [0,1].
        const float a = v.Dot(v);
        const float b = 2.0f * v.Dot(w);
        const float c = w.Dot(w) - radius * radius;

        float t0, t1;
        if (!_solveQuadratic(a, b, c, t0, t1))
            return false; // No intersection along the ray segment

        // Choose earliest valid root in [0,1].
        float tCandidate = BIGF;
        if (t0 >= 0.0f && t0 <= 1.0f) tCandidate = t0;
        if (t1 >= 0.0f && t1 <= 1.0f) tCandidate = std::min(tCandidate, t1);
        if (tCandidate == BIGF)
        return false; // Roots outside sweep interval

        tHit = tCandidate;
        const Vector2D cVec = c0 + v * tHit; // Center at impact
        const Vector2D n = cVec - p; // Outward from endpoint to center
        normalAtHit = n.Normalized(); // Unit normal

        return true; // Hit
    }

    /* ================================================================
       Moving Circle vs Static Segment (swept)
       ================================================================ */
    
   /**
   * @brief Continuous test: moving circle vs. static segment with plane hit
   * and optional endpoint checks (corner hits).
   * 
   * @return true if a collision occurs during the sweep.
   */

    bool Collision::CircleVsSegmentSweep(
        const Circle& circle,
        const Vector2D& intendedEnd,
        const LineSegment& segment,
        Vector2D& outContactPoint,
        Vector2D& outNormal,
        float& outTime,
        const bool checkEdges)
    {
        const Vector2D c0 = circle.Center; // Start center
        const Vector2D c1 = intendedEnd;   // End center
        const Vector2D v = c1 - c0;        // Motion vector

        // Signed distances to the infinite line at start and end: n dot (C - P0)
        const float d0 = segment.Normal.Dot(c0 - segment.P0);
        const float d1 = segment.Normal.Dot(c1 - segment.P0);

        // Early out -> if both are farther than radius on outward side, no hit.
        if (d0 > circle.Radius && d1 > circle.Radius) return false;

        // Check motion relative to the plane; denom is n dot v.
        const float denom = segment.Normal.Dot(v);
        if (std::fabs(denom) > EPS) {
            // Solve for t where the inflated plane is touched -> d(t) = r.
            const float t = (circle.Radius - d0) / denom;
            if (t >= 0.0f && t <= 1.0f) {
                const Vector2D c = c0 + v * t;                             // center at impact
                const Vector2D q = c - segment.Normal * circle.Radius;     // contact point on the segment line

                // Project q onto the segment to check if it lies within [P0,P1].
                const Vector2D ab = segment.P1 - segment.P0; // Segment direction
                const float abLen2 = ab.SquareLength(); // Length squared
                float s = 0.0f; // Param along [0,1]
                if (abLen2 > 0.0f)
                    s = (q - segment.P0).Dot(ab) / abLen2; // Projection param

                if (s >= 0.0f && s <= 1.0f) {
                    // Valid face hit
                    outContactPoint = q;
                    outNormal = segment.Normal; // Unit normal from segment
                    outTime = t;

                    return true;
                }
            }
        }

        // If we get here, either moving parallel or plane hit fell outside the segment.
        // Optionally test segment endpoints as circles of radius r (corner hits).
        if (checkEdges) {
            float bestT = BIGF;         // Track earliest hit time
            Vector2D bestN(0.0f, 0.0f); // Normal at earliest hit
            Vector2D bestQ(0.0f, 0.0f); // Contact point at earliest hit
            float tHit; Vector2D nHit;  // Scratch outputs from helper

            // Test P0 corner
            if (_pointSweepHitCircle(c0, c1, segment.P0, circle.Radius, tHit, nHit)) {
                if (tHit < bestT) {
                    bestT = tHit;
                    bestN = nHit;
                    bestQ = c0 + v * tHit - nHit * circle.Radius; // Contact = C - n R
                }
            }

            // Test P1 corner
            if (_pointSweepHitCircle(c0, c1, segment.P1, circle.Radius, tHit, nHit)) {
                if (tHit < bestT) {
                    bestT = tHit;
                    bestN = nHit;
                    bestQ = c0 + v * tHit - nHit * circle.Radius; // Contact = C - n R
                }
            }
            // If bestT was updated, report the corner hit.
            if (bestT < BIGF || bestT > BIGF) { // Note -> mirrors the original test
                outContactPoint = bestQ;
                outNormal = bestN;
                outTime = bestT;
                return true;
            }
        }

        return false; // No hit
    }

    /* ================================================================
       Response: reflect remaining motion about the normal
       ================================================================ */

   /**
   * @brief Reflect the remaining motion vector about a normal and update the
   * intended end position accordingly.
   */

    void Collision::CircleSegmentResponse(
        const Vector2D& contactPoint,
        const Vector2D& normal,
        Vector2D& inOutIntendedEnd,
        Vector2D& outReflectedDirection)
    {
        const Vector2D leftOver = inOutIntendedEnd - contactPoint; // remaining motion
        const float    along = leftOver.Dot(normal);               // Signed magnitude along normal
        const Vector2D reflected = leftOver - normal * (2.0f * along); // Reflect across plane

        // Mirror intended end about the surface using the reflected vector.
        inOutIntendedEnd = contactPoint + reflected;

        // Provide a unit reflected direction the caller can scale by its own speed.
        const float l2 = reflected.SquareLength();
        outReflectedDirection = (l2 > 0.0f)
            ? reflected * (1.0f / std::sqrt(l2))
            : Vector2D(0.0f, 1.0f);
    }

    /* ================================================================
       Point vs Segment (closest point + tolerance test)
       ================================================================ */

   /**
   * @brief Compute closest point on a segment to a query point and test if
   * the distance is within a given tolerance.
   * 
   * @return true if the point is within tolerance of the segment.
   */

    bool Collision::PointVsSegment(
        const Vector2D& p,
        const Vector2D& s0,
        const Vector2D& s1,
        const float tolerance,
        float* outTime,
        Vector2D* outClosest)
    {
        const Vector2D v = s1 - s0;          // Segment direction
        const float len2 = v.SquareLength(); // Squared length

        float t = 0.0f;                 // Unclamped param 
        if (len2 > 0.0f) {
            t = (p - s0).Dot(v) / len2; // Projection onto the line
        }

        // Clamp t to [0,1] explicitly without external dependencies.
        const float tc = Vector2D::ClampValue(t, 0.0f, 1.0f);
        const Vector2D q = s0 + v * tc;

        // Within tolerance if squared distance <= tol^2.
        const bool hit = (p - q).SquareLength() <= (tolerance * tolerance);

        if (outTime) *outTime = tc; // Optional outputs guarded
        if (outClosest) *outClosest = q;
        return hit;
    }

    /* ================================================================
       AABB vs AABB
       ================================================================ */

   /**
   * @brief Overlap test between two AABBs with optional manifold output.
   * 
   * @return true if overlapping, false otherwise.
   */

    bool Collision::AABBvsAABB(
        const AABB& a,
        const AABB& b,
        Vector2D* outNormal,
        float* outPenetration)
    {
        // Compute centers and half-extents.
        const Vector2D cA = (a.Min + a.Max) * 0.5f;
        const Vector2D cB = (b.Min + b.Max) * 0.5f;
        const Vector2D hA = (a.Max - a.Min) * 0.5f;
        const Vector2D hB = (b.Max - b.Min) * 0.5f;

        // Overlap on X -> total half-width minus center distance.
        const float dx = cB.X - cA.X;
        const float px = (hA.X + hB.X) - std::fabs(dx);
        if (px <= 0.0f)
            return false; // no overlap X

        // Overlap on Y
        const float dy = cB.Y - cA.Y;
        const float py = (hA.Y + hB.Y) - std::fabs(dy);
        if (py <= 0.0f)
            return false; // no overlap Y

        Vector2D n(0.0f, 0.0f); // Output normal
        float pen = 0.0f;       // Output penetration

        // Choose the axis of least penetration as the contact axis.
        if (px < py) {
            n.X = (dx < 0.0f) ? -1.0f : 1.0f; // Normal points from A to B
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

   /**
   * @brief Separate Axis Theorem overlap between triangle and AABB.
   * 
   * @return true if overlapping, false if a separating axis exists.
   */
    bool Collision::Overlap(const Triangle& tri, const AABB& box) {

        // Triangle vertices and edges.
        const Vector2D triVerts[3] = { tri.V0, tri.V1, tri.V2 };
        const Vector2D triEdges[3] = { tri.V1 - tri.V0, tri.V2 - tri.V1, tri.V0 - tri.V2 };

        // Candidate separating axes -> triangle edge normals and AABB axes.
        const Vector2D axes[5] = {
            Vector2D(-triEdges[0].Y, triEdges[0].X), // Perpendicular to edge 0
            Vector2D(-triEdges[1].Y, triEdges[1].X), // Perpendicular to edge 1
            Vector2D(-triEdges[2].Y, triEdges[2].X), // Perpendicular to edge 2
            Vector2D(1.0f, 0.0f),                    // AABB X-axis
            Vector2D(0.0f, 1.0f)                     // AABB Y-axis
        };

        // AABB corners (min->top-left->max->bottom-right)
        const Vector2D boxVerts[4] = {
            box.Min,
            Vector2D(box.Min.X, box.Max.Y),
            box.Max,
            Vector2D(box.Max.X, box.Min.Y)
        };

        // Project both shapes onto each axis and check for interval separation.
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

        return true; // No separating axis found -> overlap = 1
    }

    /* ================================================================
       Overlap: AABB vs AABB
       ================================================================ */

   /**
   * @brief Overlap test for AABB vs AABB with manifold output.
   */

    bool Collision::Overlap(const AABB& a, const AABB& b, Manifold* m) {

        // Early rejection using min/max comparisons.
        const float dx1 = b.Min.X - a.Max.X;
        const float dx2 = a.Min.X - b.Max.X;
        const float dy1 = b.Min.Y - a.Max.Y;
        const float dy2 = a.Min.Y - b.Max.Y;

        if (dx1 > 0.f || dx2 > 0.f || dy1 > 0.f || dy2 > 0.f) return false;
        if (!m) return true;// Overlap exists; caller does not need manifold

        // Compute the minimal penetration along X and Y.
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
        return true; //Overlap
    }

    /* ================================================================
       Overlap: Circle vs Circle
       ================================================================ */

   /**
   * @brief Overlap test for two circles with optional manifold output.
   */

    bool Collision::Overlap(const Circle& a, const Circle& b, Manifold* m) {

        const Vector2D d = b.Center - a.Center; // Center-to-center delta
        const float r = a.Radius + b.Radius; // Sum of radii
        const float d2 = d.SquareLength(); // Squared separation


        if (d2 > r * r) return false; // No overlap if distance > sum radii
        if (!m) return true; // Overlap but manifold not requested

        const float dLen = std::sqrt(std::max(d2, EPS));          // Avoid sqrt(0)
        m->Normal = (dLen > 0.f) ? d / dLen : Vector2D(0.f, 1.f); // Unit normal A->B
        m->Penetration = r - dLen;                                // Overlap depth
        m->Contact = b.Center - m->Normal * b.Radius;             // Contact point on B
        m->Valid = true;

        return true;
    }

    /* ================================================================
       Overlap: Circle vs AABB
       ================================================================ */

   /**
   * @brief Overlap test between a circle and an AABB with manifold output.
   */
       
    bool Collision::Overlap(const Circle& a, const AABB& b, Manifold* m)
    {
        // Clamp circle center to box to get closest point on AABB to the circle
        const float left = b.Min.X;
        const float right = b.Max.X;
        const float bottom = b.Min.Y;
        const float top = b.Max.Y;

        const float closestX = std::max(left, std::min(a.Center.X, right));
        const float closestY = std::max(bottom, std::min(a.Center.Y, top));

        const Vector2D closest(closestX, closestY); // Closest point on box
        const Vector2D toClosest = closest - a.Center; // From circle to closest
        const float d2 = toClosest.SquareLength(); // Squared distance

        // Quick reject -> if outside and farther than radius, no overlap
        if (d2 > a.Radius * a.Radius) return false;
        if (!m) return true; // Overlap exists but no manifold requested

        Vector2D normal;          // A->B normal
        float penetration = 0.0f; // Penetration depth

        //  Two cases -> center outside (or on edge/corner) vs center inside box
        if (d2 > 1e-6f) {
            // Outside -> normal is towards closest point; penetration is R - d
            const float d = std::sqrt(d2);
            normal = toClosest / d;     // Unit normal A->B
            penetration = a.Radius - d; // Overlap depth outside
        }
        else {
            // Inside -> push out along the nearest face (smallest exit distance)
            const float leftDist = a.Center.X - left;
            const float rightDist = right - a.Center.X;
            const float downDist = a.Center.Y - bottom;
            const float upDist = top - a.Center.Y;

            float minDist = leftDist;         // Track smallest exit distance
            normal = Vector2D(-1.0f, 0.0f);   // Assume left face initially

            if (rightDist < minDist) { minDist = rightDist; normal = Vector2D(1.0f, 0.0f); }
            if (downDist < minDist) { minDist = downDist;  normal = Vector2D(0.0f, -1.0f); }
            if (upDist < minDist) { minDist = upDist;    normal = Vector2D(0.0f, 1.0f); }

            // Keep normal pointing from circle to the chosen face; penetration
            // equals circle radius plus distance from center to that face.
            penetration = a.Radius + minDist;
        }

        // Contact from the invariant relation, equals 'closest' in the outside case
        const Vector2D contact = a.Center + normal * (a.Radius - penetration);

        m->Normal = normal;           // A -> B
        m->Penetration = penetration; // Depth
        m->Contact = contact;         // Contact point on the boundary
        m->Valid = true;
        return true;
    }

    void Collision::GetEdgeVertices(
        const Vector2D& center,
        const Vector2D& halfExtents,
        int edgeIndex,
        Vector2D& v1,
        Vector2D& v2)
    {
        // Calculate the 4 corners
        Vector2D corners[4] = {
            Vector2D(center.X - halfExtents.X, center.Y - halfExtents.Y), // Bottom-left
            Vector2D(center.X + halfExtents.X, center.Y - halfExtents.Y), // Bottom-right
            Vector2D(center.X + halfExtents.X, center.Y + halfExtents.Y), // Top-right
            Vector2D(center.X - halfExtents.X, center.Y + halfExtents.Y)  // Top-left
        };

        // Return edge vertices based on index
        // Edges go counter-clockwise
        switch (edgeIndex) {
        case 0: // Left edge (bottom-left to top-left)
            v1 = corners[0];
            v2 = corners[3];
            break;
        case 1: // Bottom edge (bottom-left to bottom-right)
            v1 = corners[0];
            v2 = corners[1];
            break;
        case 2: // Right edge (bottom-right to top-right)
            v1 = corners[1];
            v2 = corners[2];
            break;
        case 3: // Top edge (top-right to top-left)
            v1 = corners[2];
            v2 = corners[3];
            break;
        default:
            v1 = corners[0];
            v2 = corners[1];
            break;
        }
    }

    int Collision::ClipSegmentToLine(
        ClipVertex outVertices[2],
        const ClipVertex inVertices[2],
        const Vector2D& normal,
        float offset,
        uint8_t clipEdge)
    {
        // Start with no output points
        int numOut = 0;

        // Calculate distances from both vertices to the clipping line
        // distance = dot(vertex, normal) - offset
        // Positive distance = outside (keep)
        // Negative distance = inside (clip)
        float distance0 = Dot(inVertices[0].v, normal) - offset;
        float distance1 = Dot(inVertices[1].v, normal) - offset;

        // If a vertex is behind the plane (distance <= 0), keep it
        if (distance0 <= 0.0f) {
            outVertices[numOut++] = inVertices[0];
        }

        if (distance1 <= 0.0f) {
            outVertices[numOut++] = inVertices[1];
        }

        // If the segment crosses the plane, add the intersection point
        if (distance0 * distance1 < 0.0f) {
            // Segment crosses the plane
            // Linear interpolation to find intersection
            float interp = distance0 / (distance0 - distance1);

            ClipVertex& cv = outVertices[numOut];
            cv.v.X = inVertices[0].v.X + interp * (inVertices[1].v.X - inVertices[0].v.X);
            cv.v.Y = inVertices[0].v.Y + interp * (inVertices[1].v.Y - inVertices[0].v.Y);

            // Track which edge we clipped against
            cv.edgeIndex = clipEdge;

            ++numOut;
        }

        return numOut;
    }

    Collision::ContactManifold Collision::GenerateBoxBoxManifold(
        const Vector2D& centerA,
        const Vector2D& halfExtentsA,
        const Vector2D& centerB,
        const Vector2D& halfExtentsB,
        const Vector2D& normal,
        float penetration)
    {
        ContactManifold manifold;
        manifold.normal = normal;
        manifold.penetration = penetration;
        manifold.pointCount = 0;

        // ========================================================================
        // STEP 1: Find Reference Face (most perpendicular to collision normal)
        // ========================================================================

        // For AABB, we have 4 face normals: left, bottom, right, top
        // Face normals for box A
        Vector2D faceNormalsA[4] = {
            Vector2D(-1, 0),  // Left face
            Vector2D(0, -1),  // Bottom face
            Vector2D(1, 0),   // Right face
            Vector2D(0, 1)    // Top face
        };

        // Find which face of A is most perpendicular to the collision normal
        // (largest absolute dot product with normal)
        float maxDotA = -FLT_MAX;
        int refFaceA = 0;

        for (int i = 0; i < 4; ++i) {
            float dot = std::abs(Dot(faceNormalsA[i], normal));
            if (dot > maxDotA) {
                maxDotA = dot;
                refFaceA = i;
            }
        }

        // Face normals for box B
        Vector2D faceNormalsB[4] = {
            Vector2D(-1, 0),  // Left face
            Vector2D(0, -1),  // Bottom face
            Vector2D(1, 0),   // Right face
            Vector2D(0, 1)    // Top face
        };

        // Find which face of B is most perpendicular to the collision normal
        float maxDotB = -FLT_MAX;
        int refFaceB = 0;

        for (int i = 0; i < 4; ++i) {
            // Note: We use -normal because we want the face pointing TOWARD A
            float dot = std::abs(Dot(faceNormalsB[i], normal * -1.0f));
            if (dot > maxDotB) {
                maxDotB = dot;
                refFaceB = i;
            }
        }

        // ========================================================================
        // STEP 2: Determine Reference and Incident
        // ========================================================================

        // The box with the face more perpendicular to the normal is the reference
        bool flipNormal = false;
        int refEdge, incEdge;
        Vector2D refCenter, incCenter;
        Vector2D refHalfExtents, incHalfExtents;

        if (maxDotA > maxDotB) {
            // Box A has the reference face
            refEdge = refFaceA;
            refCenter = centerA;
            refHalfExtents = halfExtentsA;

            // Box B has the incident face (opposite side)
            incEdge = refFaceB;
            incCenter = centerB;
            incHalfExtents = halfExtentsB;

            flipNormal = false;
        }
        else {
            // Box B has the reference face
            refEdge = refFaceB;
            refCenter = centerB;
            refHalfExtents = halfExtentsB;

            // Box A has the incident face
            incEdge = refFaceA;
            incCenter = centerA;
            incHalfExtents = halfExtentsA;

            flipNormal = true;  // Normal points from A to B, but ref is now B
        }

        // ========================================================================
        // STEP 3: Get Edge Vertices
        // ========================================================================

        // Reference edge vertices
        Vector2D refV1, refV2;
        GetEdgeVertices(refCenter, refHalfExtents, refEdge, refV1, refV2);

        // Incident edge vertices
        Vector2D incV1, incV2;
        GetEdgeVertices(incCenter, incHalfExtents, incEdge, incV1, incV2);

        // ========================================================================
        // STEP 4: Clip Incident Edge Against Reference Face's Side Planes
        // ========================================================================

        // Reference edge vector and normal
        Vector2D refEdgeVec;
        refEdgeVec.X = refV2.X - refV1.X;
        refEdgeVec.Y = refV2.Y - refV1.Y;

        // Normalize reference edge
        float refLength = std::sqrt(refEdgeVec.X * refEdgeVec.X + refEdgeVec.Y * refEdgeVec.Y);
        if (refLength > 0.0f) {
            refEdgeVec.X /= refLength;
            refEdgeVec.Y /= refLength;
        }

        // Reference face normal (perpendicular to edge, pointing outward)
        Vector2D refNormal;
        refNormal.X = refEdgeVec.Y;
        refNormal.Y = -refEdgeVec.X;

        // If we flipped, adjust normal direction
        if (flipNormal) {
            refNormal.X = -refNormal.X;
            refNormal.Y = -refNormal.Y;
        }

        // Side normals (perpendicular to reference edge)
        Vector2D sideNormal1;
        sideNormal1.X = -refEdgeVec.X;
        sideNormal1.Y = -refEdgeVec.Y;

        Vector2D sideNormal2 = refEdgeVec;

        // Side plane offsets
        float sideOffset1 = Dot(sideNormal1, refV1);
        float sideOffset2 = Dot(sideNormal2, refV2);

        // Clip incident edge against side planes
        ClipVertex incidentEdge[2];
        incidentEdge[0].v = incV1;
        incidentEdge[0].edgeIndex = static_cast<uint8_t>(incEdge);
        incidentEdge[1].v = incV2;
        incidentEdge[1].edgeIndex = static_cast<uint8_t>(incEdge);

        // Clip against side 1
        ClipVertex clipPoints1[2];
        int numClip1 = ClipSegmentToLine(clipPoints1, incidentEdge, sideNormal1, sideOffset1, static_cast<uint8_t>(refEdge));

        if (numClip1 < 2) {
            // Edge is completely clipped, no contact
            return manifold;
        }

        // Clip against side 2
        ClipVertex clipPoints2[2];
        int numClip2 = ClipSegmentToLine(clipPoints2, clipPoints1, sideNormal2, sideOffset2, static_cast<uint8_t>(refEdge + 1));

        if (numClip2 < 1) {
            // Edge is completely clipped, no contact
            return manifold;
        }

        // ========================================================================
        // STEP 5: Keep Points Below Reference Face
        // ========================================================================

        // Reference face offset
        float refOffset = Dot(refNormal, refV1);

        // Add points that are below the reference face
        for (int i = 0; i < numClip2; ++i) {
            float separation = Dot(clipPoints2[i].v, refNormal) - refOffset;

            if (separation <= 0.0f) {
                // Point is penetrating
                manifold.points[manifold.pointCount] = clipPoints2[i].v;

                // Store feature IDs for contact tracking
                manifold.ids[manifold.pointCount].inEdge1 = static_cast<uint8_t>(refEdge);
                manifold.ids[manifold.pointCount].outEdge1 = clipPoints2[i].edgeIndex;
                manifold.ids[manifold.pointCount].inEdge2 = clipPoints2[i].edgeIndex;
                manifold.ids[manifold.pointCount].outEdge2 = static_cast<uint8_t>(refEdge);

                ++manifold.pointCount;

                // Maximum 2 points in 2D
                if (manifold.pointCount >= 2) break;
            }
        }

        return manifold;
    }

     /* ================================================================
       Future use
       ================================================================ */

    ///* ================================================================
    //   Overlap: ConvexPolygon vs ConvexPolygon
    //   ================================================================ */
    //bool Collision::Overlap(const ConvexPolygon& a, const ConvexPolygon& b, Manifold* m) {
    //    // Implementation omitted for brevity
    //    return false;
    //}

    ///* ================================================================
    //   Sweep: AABB vs AABB
    //   ================================================================ */
    //Collision::SweepHit Collision::Sweep(const AABB& a, const Vector2D& aEnd,
    //    const AABB& b, const Vector2D& bEnd) {
    //    // Implementation omitted for brevity
    //    return {};
    //}

    ///* ================================================================
    //   Sweep: Circle vs Circle
    //   ================================================================ */
    //Collision::SweepHit Collision::Sweep(const Circle& a, const Vector2D& aEnd,
    //    const Circle& b, const Vector2D& bEnd) {
    //    // Implementation omitted for brevity
    //    return {};
    //}

    ///* ================================================================
    //   Sweep: Circle vs AABB
    //   ================================================================ */
    //Collision::SweepHit Collision::Sweep(const Circle& a, const Vector2D& aEnd,
    //    const AABB& b, const Vector2D& bEnd) {
    //    // Implementation omitted for brevity
    //    return {};
    //}
}
