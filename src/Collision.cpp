#include "Collision.h"

namespace {
    constexpr float EPS = 1e-6f;
    constexpr float BIGF = std::numeric_limits<float>::max();
}

/* ================================================================
   Builders
   ================================================================ */

Collision::LineSegment Collision::MakeSegment(const glm::vec2& p0, const glm::vec2& p1) {
    LineSegment s;
    s.p0 = p0;
    s.p1 = p1;

    glm::vec2 edge = p1 - p0;

    // Legacy convention: outward normal from (edge.y, -edge.x)
    glm::vec2 n(edge.y, -edge.x);
    float L2 = glm::length2(n);
    s.normal = (L2 > 0.0f) ? n * (1.0f / std::sqrt(L2)) : glm::vec2(0.0f, 1.0f);
    return s;
}

/* ================================================================
   Quadratic solver (stable enough for our use)
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
   Used to catch corner hits when plane hit falls outside segment
   ================================================================ */
bool Collision::pointSweepHitCircle(const glm::vec2& C0, const glm::vec2& C1,
    const glm::vec2& P, float radius,
    float& tHit, glm::vec2& normalAtHit)
{
    glm::vec2 v = C1 - C0;         // motion
    glm::vec2 w = C0 - P;          // from endpoint to start center

    float a = glm::dot(v, v);
    float b = 2.0f * glm::dot(v, w);
    float c = glm::dot(w, w) - radius * radius;

    float t0, t1;
    if (!solveQuadratic(a, b, c, t0, t1)) return false;

    // choose earliest valid root in [0,1]
    float tCandidate = BIGF;
    if (t0 >= 0.0f && t0 <= 1.0f) tCandidate = t0;
    if (t1 >= 0.0f && t1 <= 1.0f) tCandidate = std::min(tCandidate, t1);
    if (tCandidate == BIGF) return false;

    tHit = tCandidate;
    glm::vec2 C = C0 + v * tHit;
    glm::vec2 n = C - P; // outward from endpoint to circle center
    float L2 = glm::length2(n);
    normalAtHit = (L2 > 0.0f) ? (n * (1.0f / std::sqrt(L2))) : glm::vec2(0.0f, 1.0f);
    return true;
}

/* ================================================================
   Moving Circle vs Static Segment (swept)
   ================================================================ */
bool Collision::CircleVsSegmentSweep(
    const Circle& circle,
    const glm::vec2& intendedEnd,
    const LineSegment& seg,
    glm::vec2& outContactPoint,
    glm::vec2& outNormal,
    float& outTime,
    bool checkEdges)
{
    const glm::vec2 C0 = circle.center;
    const glm::vec2 C1 = intendedEnd;
    const glm::vec2 v = C1 - C0;

    // Signed distances to line (plane) at start and end: n·(C - p0)
    float d0 = glm::dot(seg.normal, C0 - seg.p0);
    float d1 = glm::dot(seg.normal, C1 - seg.p0);

    // If both start and end are farther than radius on the outward side, no hit
    if (d0 > circle.radius && d1 > circle.radius) return false;

    // Motion parallel to plane?
    float denom = glm::dot(seg.normal, v);
    if (std::fabs(denom) > EPS) {
        // Solve for t where the *inflated* plane is touched: d(t) = r
        float t = (circle.radius - d0) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            glm::vec2 C = C0 + v * t;             // center at impact
            glm::vec2 Q = C - seg.normal * circle.radius; // contact point on the segment line

            // Check if Q projects inside segment extents
            glm::vec2 ab = seg.p1 - seg.p0;
            float abLen2 = glm::length2(ab);
            float s = 0.0f;
            if (abLen2 > 0.0f) s = glm::dot(Q - seg.p0, ab) / abLen2;

            if (s >= 0.0f && s <= 1.0f) {
                outContactPoint = Q;
                outNormal = seg.normal;  // normal already unit
                outTime = t;
                return true;
            }
        }
    }

    // If we get here, we either moved parallel OR plane hit fell outside segment.
    // Optionally test segment endpoints as circles of radius r (corner hits).
    if (checkEdges) {
        float bestT = BIGF;
        glm::vec2 bestN(0.0f), bestQ(0.0f);

        float tHit; glm::vec2 nHit;

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
    const glm::vec2& contactPoint,
    const glm::vec2& normal,
    glm::vec2& inOutIntendedEnd,
    glm::vec2& outReflectedDir)
{
    glm::vec2 leftOver = inOutIntendedEnd - contactPoint; // remaining motion
    float along = glm::dot(leftOver, normal);
    glm::vec2 reflected = leftOver - 2.0f * along * normal;

    // Update intended end to be mirrored about the surface
    inOutIntendedEnd = contactPoint + reflected;

    // Unit reflected direction for caller to reuse with its own speed
    float L2 = glm::length2(reflected);
    outReflectedDir = (L2 > 0.0f) ? reflected * (1.0f / std::sqrt(L2)) : glm::vec2(0.0f, 1.0f);
}

/* ================================================================
   Point vs Segment (closest point + tolerance test)
   ================================================================ */
bool Collision::PointVsSegment(
    const glm::vec2& P,
    const glm::vec2& S0,
    const glm::vec2& S1,
    float tol,
    float* outT,
    glm::vec2* outClosest)
{
    glm::vec2 V = S1 - S0;
    float len2 = glm::length2(V);

    float t = 0.0f;
    if (len2 > 0.0f) {
        t = glm::dot(P - S0, V) / len2;
    }
    float tc = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    glm::vec2 Q = S0 + V * tc;

    bool hit = glm::distance2(P, Q) <= (tol * tol);

    if (outT)       *outT = tc;
    if (outClosest) *outClosest = Q;
    return hit;
}

/* ================================================================
   AABB vs AABB (overlap + minimal manifold)
   ================================================================ */
bool Collision::AABBvsAABB(
    const AABB& A,
    const AABB& B,
    glm::vec2* outNormal,
    float* outPenetration)
{
    // Centers and half extents
    glm::vec2 cA = 0.5f * (A.min + A.max);
    glm::vec2 cB = 0.5f * (B.min + B.max);
    glm::vec2 hA = 0.5f * (A.max - A.min);
    glm::vec2 hB = 0.5f * (B.max - B.min);

    float dx = cB.x - cA.x;
    float px = (hA.x + hB.x) - std::fabs(dx);
    if (px <= 0.0f) return false; // no overlap X

    float dy = cB.y - cA.y;
    float py = (hA.y + hB.y) - std::fabs(dy);
    if (py <= 0.0f) return false; // no overlap Y

    glm::vec2 n(0.0f);
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
