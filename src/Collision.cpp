#include "Collision.h"
#include <cmath>      // std::sqrt, std::fabs
#include <limits>     // std::numeric_limits
#include <algorithm>  // std::min
#include "Math/Vector2D.h"

namespace {
    constexpr float EPS = 1e-6f;
    constexpr float BIGF = std::numeric_limits<float>::max();
}

/* ================================================================
   Builders
   ================================================================ */
Collision::LineSegment Collision::MakeSegment(const Vector2D& p0, const Vector2D& p1) {
    LineSegment s;
    s.p0 = p0;
    s.p1 = p1;

    Vector2D edge = p1 - p0;

    // Legacy convention: outward normal from (edge.y, -edge.x), then normalize
    Vector2D n(edge.y, -edge.x);
    float L2 = n.SquareLength();
    s.normal = (L2 > 0.0f) ? n * (1.0f / std::sqrt(L2)) : Vector2D(0.0f, 1.0f);
    return s;
}

/* ================================================================
   Quadratic solver
   ================================================================ */
bool Collision::solveQuadratic(float a, float b, float c, float& t0, float& t1) {
    if (std::fabs(a) < EPS) {
        if (std::fabs(b) < EPS) return false;
        t0 = t1 = -c / b;
        return true;
    }
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float sqrtD = std::sqrt(disc);
    float q = (b > 0.0f) ? -0.5f * (b + sqrtD) : -0.5f * (b - sqrtD);
    t0 = q / a;
    t1 = c / q;
    if (t0 > t1) std::swap(t0, t1);
    return true;
}

/* ================================================================
   Swept point (circle center) vs point (segment endpoint)
   ================================================================ */
bool Collision::pointSweepHitCircle(const Vector2D& C0, const Vector2D& C1,
    const Vector2D& P, float radius,
    float& tHit, Vector2D& normalAtHit)
{
    Vector2D v = C1 - C0;  // motion
    Vector2D w = C0 - P;   // from endpoint to start center

    float a = v.Dot(v);
    float b = 2.0f * v.Dot(w);
    float c = w.Dot(w) - radius * radius;

    float t0, t1;
    if (!solveQuadratic(a, b, c, t0, t1)) return false;

    // earliest valid root in [0,1]
    float tCandidate = BIGF;
    if (t0 >= 0.0f && t0 <= 1.0f) tCandidate = t0;
    if (t1 >= 0.0f && t1 <= 1.0f) tCandidate = std::min(tCandidate, t1);
    if (tCandidate == BIGF) return false;

    tHit = tCandidate;
    Vector2D C = C0 + v * tHit;
    Vector2D n = C - P; // outward from endpoint to circle center
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
    const LineSegment& seg,
    Vector2D& outContactPoint,
    Vector2D& outNormal,
    float& outTime,
    bool checkEdges)
{
    const Vector2D C0 = circle.center;
    const Vector2D C1 = intendedEnd;
    const Vector2D v = C1 - C0;

    // Signed distances to line (plane) at start and end: n·(C - p0)
    float d0 = seg.normal.Dot(C0 - seg.p0);
    float d1 = seg.normal.Dot(C1 - seg.p0);

    // If both start and end are farther than radius on the outward side, no hit
    if (d0 > circle.radius && d1 > circle.radius) return false;

    // Motion parallel to plane?
    float denom = seg.normal.Dot(v);
    if (std::fabs(denom) > EPS) {
        // Solve for t where the *inflated* plane is touched: d(t) = r
        float t = (circle.radius - d0) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            Vector2D C = C0 + v * t;                         // center at impact
            Vector2D Q = C - seg.normal * circle.radius;     // contact point on the segment line

            // Check if Q projects inside segment extents
            Vector2D ab = seg.p1 - seg.p0;
            float abLen2 = ab.SquareLength();
            float s = 0.0f;
            if (abLen2 > 0.0f) s = (Q - seg.p0).Dot(ab) / abLen2;

            if (s >= 0.0f && s <= 1.0f) {
                outContactPoint = Q;
                outNormal = seg.normal;  // unit
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

        if (pointSweepHitCircle(C0, C1, seg.p0, circle.radius, tHit, nHit)) {
            if (tHit < bestT) {
                bestT = tHit;
                bestN = nHit;
                bestQ = C0 + v * tHit - nHit * circle.radius;
            }
        }
        if (pointSweepHitCircle(C0, C1, seg.p1, circle.radius, tHit, nHit)) {
            if (tHit < bestT) {
                bestT = tHit;
                bestN = nHit;
                bestQ = C0 + v * tHit - nHit * circle.radius;
            }
        }

        if (bestT != BIGF) {
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
    Vector2D& outReflectedDir)
{
    Vector2D leftOver = inOutIntendedEnd - contactPoint; // remaining motion
    float    along = leftOver.Dot(normal);
    Vector2D reflected = leftOver - normal * (2.0f * along);

    // Update intended end to be mirrored about the surface
    inOutIntendedEnd = contactPoint + reflected;

    // Unit reflected direction for caller to reuse with its own speed
    float L2 = reflected.SquareLength();
    outReflectedDir = (L2 > 0.0f) ? reflected * (1.0f / std::sqrt(L2)) : Vector2D(0.0f, 1.0f);
}

/* ================================================================
   Point vs Segment (closest point + tolerance test)
   ================================================================ */
bool Collision::PointVsSegment(
    const Vector2D& P,
    const Vector2D& S0,
    const Vector2D& S1,
    float tol,
    float* outT,
    Vector2D* outClosest)
{
    Vector2D V = S1 - S0;
    float    len2 = V.SquareLength();

    float t = 0.0f;
    if (len2 > 0.0f) {
        t = (P - S0).Dot(V) / len2;
    }
    // clamp to [0,1] without relying on glm
    float tc = Vector2D::ClampValue(t, 0.0f, 1.0f);
    Vector2D Q = S0 + V * tc;

    bool hit = (P - Q).SquareLength() <= (tol * tol);

    if (outT)       *outT = tc;
    if (outClosest) *outClosest = Q;
    return hit;
}

/* ================================================================
   AABB vs AABB 
   ================================================================ */
bool Collision::AABBvsAABB(
    const AABB& A,
    const AABB& B,
    Vector2D* outNormal,
    float* outPenetration)
{
    // Centers and half extents
    Vector2D cA = (A.min + A.max) * 0.5f;
    Vector2D cB = (B.min + B.max) * 0.5f;
    Vector2D hA = (A.max - A.min) * 0.5f;
    Vector2D hB = (B.max - B.min) * 0.5f;

    float dx = cB.x - cA.x;
    float px = (hA.x + hB.x) - std::fabs(dx);
    if (px <= 0.0f) return false; // no overlap X

    float dy = cB.y - cA.y;
    float py = (hA.y + hB.y) - std::fabs(dy);
    if (py <= 0.0f) return false; // no overlap Y

    Vector2D n(0.0f, 0.0f);
    float pen = 0.0f;

    if (px < py) {
        n.x = (dx < 0.0f) ? -1.0f : 1.0f;
        pen = px;
    }
    else {
        n.y = (dy < 0.0f) ? -1.0f : 1.0f;
        pen = py;
    }

    if (outNormal)      *outNormal = n;
    if (outPenetration) *outPenetration = pen;
    return true;
}
