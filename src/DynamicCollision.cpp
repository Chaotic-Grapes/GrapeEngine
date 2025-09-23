#include "Include/DynamicCollision.h"
#include <algorithm>
#include <cmath>

//helpers for squared length and float clamp
static inline float Dot(const Vector2D& a, const Vector2D& b) { return a.x * b.x + a.y * b.y; }
static inline float Len2(const Vector2D& v) { return Dot(v, v); }
static inline float Clamp01(float t) { return (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t); }


// discrete means at the moment current frame
// discrete AABB vs AABB 
bool DynCol::Overlap(const AABB& A, const AABB& B, Manifold* m) {
    const float dx1 = B.min.x - A.max.x; // separation if B is right of A
    const float dx2 = A.min.x - B.max.x; // separation if A is right of B
    const float dy1 = B.min.y - A.max.y;
    const float dy2 = A.min.y - B.max.y;

    if (dx1 > 0.f || dx2 > 0.f || dy1 > 0.f || dy2 > 0.f) return false;

    if (!m) return true;

    // minimal translation along axis of least penetration
    float px = std::min(A.max.x - B.min.x, B.max.x - A.min.x);
    float py = std::min(A.max.y - B.min.y, B.max.y - A.min.y);
    if (px < py) {
        m->normal = { (A.max.x + A.min.x < B.max.x + B.min.x) ? -1.f : 1.f, 0.f };
        m->penetration = px;
    }
    else {
        m->normal = { 0.f, (A.max.y + A.min.y < B.max.y + B.min.y) ? -1.f : 1.f };
        m->penetration = py;
    }
    m->valid = true;
    return true;
}

//discrete circle vs circle
bool DynCol::Overlap(const Circle& A, const Circle& B, Manifold* m) {
    Vector2D d = { A.c.x - B.c.x, A.c.y - B.c.y };
    float r = A.r + B.r;
    float d2 = Len2(d);
    if (d2 > r * r) return false;
    if (!m) return true;

    float dlen = std::sqrt(std::max(d2, 1e-12f));
    m->normal = (dlen > 0.f) ? Vector2D{ d.x / dlen, d.y / dlen } : Vector2D{ 0.f,1.f };
    m->penetration = r - dlen;
    m->contact = { B.c.x + m->normal.x * B.r, B.c.y + m->normal.y * B.r };
    m->valid = true;
    return true;
}

//discrete circle vs AABB
bool DynCol::Overlap(const Circle& A, const AABB& B, Manifold* m) {
    // closest point on box to circle center
    float cx = std::min(std::max(A.c.x, B.min.x), B.max.x);
    float cy = std::min(std::max(A.c.y, B.min.y), B.max.y);
    Vector2D q{ cx, cy };
    Vector2D d{ A.c.x - q.x, A.c.y - q.y };

    float d2 = Len2(d);
    if (d2 > A.r * A.r) return false;
    if (!m) return true;

    float dlen = std::sqrt(std::max(d2, 1e-12f));
    Vector2D n = (dlen > 0.f) ? Vector2D{ d.x / dlen, d.y / dlen } : Vector2D{ 0.f,1.f };

    // If center is inside box, push out along smallest axis
    bool inside = (A.c.x >= B.min.x && A.c.x <= B.max.x && A.c.y >= B.min.y && A.c.y <= B.max.y);
    if (inside) {
        float dx = std::min(A.c.x - B.min.x, B.max.x - A.c.x);
        float dy = std::min(A.c.y - B.min.y, B.max.y - A.c.y);
        if (dx < dy) n = { (A.c.x - (B.min.x + B.max.x) * 0.5f) < 0.f ? -1.f : 1.f, 0.f };
        else         n = { 0.f, (A.c.y - (B.min.y + B.max.y) * 0.5f) < 0.f ? -1.f : 1.f };
        m->penetration = (dx < dy ? dx : dy) + A.r; // move circle out to surface
        m->contact = { A.c.x - n.x * A.r, A.c.y - n.y * A.r };
    }
    else {
        m->penetration = A.r - dlen;
        m->contact = q;
    }

    m->normal = n;
    m->valid = true;
    return true;
}

//discrete convex vs convex
static bool projectOntoAxis(const std::vector<Vector2D>& verts, const Vector2D& axis, float& outMin, float& outMax) {
    float p = Dot(verts[0], axis);
    outMin = outMax = p;
    for (size_t i = 1; i < verts.size(); ++i) {
        p = Dot(verts[i], axis);
        outMin = std::min(outMin, p);
        outMax = std::max(outMax, p);
    }
    return true;
}

bool DynCol::Overlap(const ConvexPolygon& A, const ConvexPolygon& B, Manifold* m) {
    float minPen = FLT_MAX;
    Vector2D bestN{ 0.f,0.f };

    auto checkAxes = [&](const std::vector<Vector2D>& vs) -> bool {
        for (size_t i = 0; i < vs.size(); ++i) {
            Vector2D e{ vs[(i + 1) % vs.size()].x - vs[i].x, vs[(i + 1) % vs.size()].y - vs[i].y };
            Vector2D n{ -e.y, e.x }; // edge normal
            float nlen2 = Len2(n); if (nlen2 < 1e-12f) continue;
            n = { n.x / std::sqrt(nlen2), n.y / std::sqrt(nlen2) };

            float aMin, aMax, bMin, bMax;
            projectOntoAxis(A.verts, n, aMin, aMax);
            projectOntoAxis(B.verts, n, bMin, bMax);

            if (aMax < bMin || bMax < aMin) return false; // separating axis

            float pen = std::min(aMax - bMin, bMax - aMin);
            if (pen < minPen) { minPen = pen; bestN = n; }
        }
        return true;
        };

    if (!checkAxes(A.verts)) return false;
    if (!checkAxes(B.verts)) return false;

    if (m) {
        // Make normal point from B->A: compare polygon centroids
        Vector2D cA{ 0,0 }, cB{ 0,0 };
        for (auto& v : A.verts) { cA.x += v.x; cA.y += v.y; }
        for (auto& v : B.verts) { cB.x += v.x; cB.y += v.y; }
        cA.x /= (float)A.verts.size(); cA.y /= (float)A.verts.size();
        cB.x /= (float)B.verts.size(); cB.y /= (float)B.verts.size();
        Vector2D dir{ cA.x - cB.x, cA.y - cB.y };
        if (Dot(dir, bestN) < 0.f) { bestN.x = -bestN.x; bestN.y = -bestN.y; }

        m->normal = bestN;
        m->penetration = std::max(0.f, minPen);
        m->valid = true;
    }
    return true;
}

//swept AABB vs AABB
DynCol::SweepHit DynCol::Sweep(const AABB& A, const Vector2D& A_end,
    const AABB& B, const Vector2D& B_end)
{
    SweepHit H;
    Vector2D vA{ A_end.x - ((A.min.x + A.max.x) * 0.5f), A_end.y - ((A.min.y + A.max.y) * 0.5f) };
    Vector2D vB{ B_end.x - ((B.min.x + B.max.x) * 0.5f), B_end.y - ((B.min.y + B.max.y) * 0.5f) };
    Vector2D vRel{ vA.x - vB.x, vA.y - vB.y };

    // Expand B by A’s half extents (Minkowski sum) and test point vs expanded box:
    Vector2D hA{ (A.max.x - A.min.x) * 0.5f, (A.max.y - A.min.y) * 0.5f };
    DynCol::AABB E; // expanded box with center at B
    Vector2D cB{ (B.min.x + B.max.x) * 0.5f, (B.min.y + B.max.y) * 0.5f };
    E.min = { B.min.x - hA.x, B.min.y - hA.y };
    E.max = { B.max.x + hA.x, B.max.y + hA.y };

    // Start point is A’s center
    Vector2D p0{ (A.min.x + A.max.x) * 0.5f, (A.min.y + A.max.y) * 0.5f };
    Vector2D p1{ p0.x + vRel.x, p0.y + vRel.y };

    // Ray cast p0->p1 against expanded AABB
    float tEnter = 0.f, tExit = 1.f;
    Vector2D nEnter{ 0,0 };

    auto slab = [&](float p0, float p1, float bmin, float bmax, float nx, float ny)->bool {
        float d = p1 - p0;
        if (std::fabs(d) < 1e-12f) {
            // parallel: outside?
            return (p0 >= bmin && p0 <= bmax);
        }
        float invd = 1.f / d;
        float t0 = (bmin - p0) * invd;
        float t1 = (bmax - p0) * invd;
        float tmin = std::min(t0, t1);
        float tmax = std::max(t0, t1);
        // track normal at entry plane
        Vector2D n = (t0 < t1) ? Vector2D{ nx, ny } : Vector2D{ -nx, -ny };

        if (tmin > tEnter) { tEnter = tmin; nEnter = n; }
        if (tmax < tExit) { tExit = tmax; }
        return tEnter <= tExit;
        };

    if (!slab(p0.x, p1.x, E.min.x, E.max.x, -1.f, 0.f)) return H;
    if (!slab(p0.y, p1.y, E.min.y, E.max.y, 0.f, -1.f)) return H;

    H.hit = (tEnter >= 0.f && tEnter <= 1.f);
    if (H.hit) {
        H.toi = tEnter;
        float nlen2 = Len2(nEnter);
        if (nlen2 > 0.f) { float s = 1.f / std::sqrt(nlen2); nEnter.x *= s; nEnter.y *= s; }
        H.normal = nEnter; // points from B -> A
    }
    return H;
}

//swept circle vs circle
DynCol::SweepHit DynCol::Sweep(const Circle& A, const Vector2D& A_end,
    const Circle& B, const Vector2D& B_end)
{
    SweepHit H;

    Vector2D vA{ A_end.x - A.c.x, A_end.y - A.c.y };
    Vector2D vB{ B_end.x - B.c.x, B_end.y - B.c.y };
    Vector2D v{ vA.x - vB.x, vA.y - vB.y }; // relative motion (A wrt B)

    Vector2D w0{ A.c.x - B.c.x, A.c.y - B.c.y }; // initial offset
    float R = A.r + B.r;

    float a = Len2(v);
    float b = 2.f * Dot(v, w0);
    float c = Len2(w0) - R * R;

    // Handle degenerate (no relative motion)
    if (std::fabs(a) < 1e-12f) {
        if (c <= 0.f) {
            // Already overlapping
            H.hit = true; H.toi = 0.f;
            Vector2D n = w0;
            float L2 = Len2(n); if (L2 > 0.f) { float s = 1.f / std::sqrt(L2); n.x *= s; n.y *= s; }
            else n = { 0.f,1.f };
            H.normal = n;
        }
        return H;
    }

    float disc = b * b - 4.f * a * c;
    if (disc < 0.f) return H;

    float sqrtD = std::sqrt(disc);
    float t0 = (-b - sqrtD) / (2.f * a);
    float t1 = (-b + sqrtD) / (2.f * a);
    if (t0 > t1) std::swap(t0, t1);

    // earliest valid in [0,1]
    float tHit = 10.f;
    if (t0 >= 0.f && t0 <= 1.f) tHit = t0;
    if (t1 >= 0.f && t1 <= 1.f) tHit = std::min(tHit, t1);

    if (tHit > 1.f) return H;

    H.hit = true;
    H.toi = tHit;

    Vector2D Ac{ A.c.x + vA.x * tHit, A.c.y + vA.y * tHit };
    Vector2D Bc{ B.c.x + vB.x * tHit, B.c.y + vB.y * tHit };
    Vector2D n{ Ac.x - Bc.x, Ac.y - Bc.y };
    float L2 = Len2(n);
    if (L2 > 0.f) { float s = 1.f / std::sqrt(L2); n.x *= s; n.y *= s; }
    else n = { 0.f,1.f };
    H.normal = n; // from B -> A at impact
    return H;
}

//swept circle vs AABB
DynCol::SweepHit DynCol::Sweep(const Circle& A, const Vector2D& A_end,
    const AABB& B, const Vector2D& B_end)
{
    // Expand box by circle radius
    AABB E;
    E.min = { B.min.x - A.r, B.min.y - A.r };
    E.max = { B.max.x + A.r, B.max.y + A.r };

    // Cast point A.c vs expanded box using the same slab routine as AABB sweep
    // Build a unit AABB with A centered; reuse the AABB sweep by giving half-extents/ZERO
    AABB AA;
    AA.min = { A.c.x, A.c.y };
    AA.max = { A.c.x, A.c.y };

    Vector2D Aend = A_end;
    Vector2D Bend = { (B.min.x + B.max.x) * 0.5f, (B.min.y + B.max.y) * 0.5f }; // no movement
    return Sweep(AA, Aend, E, Bend);
}
