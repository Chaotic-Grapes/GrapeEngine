/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu 
 * 
 * @file    DynamicCollision.cpp
 * @brief   Discrete overlap tests and swept (continuous) collision queries for 2D shapes.
 *
 * @details Implements:
 *   • SAT overlap for Triangle vs AABB (triangle edge normals + box axes). :contentReference[oaicite:10]{index=10}
 *   • Discrete overlap:
 *       - AABB vs AABB (with minimal translation vector in Manifold). :contentReference[oaicite:11]{index=11}
 *       - Circle vs Circle (normal, penetration, contact). :contentReference[oaicite:12]{index=12}
 *       - Circle vs AABB (closest point, inside/outside handling). :contentReference[oaicite:13]{index=13}
 *       - Convex vs Convex (edge normals on both polygons). :contentReference[oaicite:14]{index=14}
 *   • Swept tests (continuous):
 *       - AABB vs AABB using Minkowski expansion + slab raycast. :contentReference[oaicite:15]{index=15}
 *       - Circle vs Circle analytical TOI (quadratic). :contentReference[oaicite:16]{index=16}
 *       - Circle vs AABB via box expansion by radius + AABB sweep reuse. :contentReference[oaicite:17]{index=17}
 *
 * @usage
 *   - Use Overlap(...) for discrete frame-by-frame tests (broad/narrow phase).
 *   - Use Sweep(...) to compute time of impact and separating normal for CCD (continuous collision detection).
 *
 * @notes
 *   - Helper functions provide dot/length/clamp/perp and polygon projection. :contentReference[oaicite:18]{index=18}
 *   - All normals in sweep results point from target (B) to mover (A) at impact. :contentReference[oaicite:19]{index=19}
 *
 * @dependencies
 *   - DynamicCollision.h, Vector2D, <algorithm>, <cmath>. :contentReference[oaicite:20]{index=20}
 *
 */

#include "DynamicCollision.h"
#include <algorithm>
#include <cmath>

//helpers for squared length and float clamp
static inline float Dot(const Vector2D& a, const Vector2D& b) { return a.X * b.X + a.Y * b.Y; }
static inline float Len2(const Vector2D& v) { return Dot(v, v); }
static inline float Clamp01(float t) { return (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t); }

static inline Vector2D perp(const Vector2D& e) { return Vector2D(-e.Y, e.X); } // 90° CCW

static inline void projectPolygonOnAxis(const Vector2D* verts, int count,
    const Vector2D& axis, float& outMin, float& outMax)
{
    
    float p = verts[0].Dot(axis);
    outMin = outMax = p;
    for (int i = 1; i < count; ++i) {
        p = verts[i].Dot(axis);
        outMin = std::min(outMin, p);
        outMax = std::max(outMax, p);
    }
}

static inline void boxCorners(const DynCol::AABB& b, Vector2D out[4]) {
    out[0] = { b.min.X, b.min.Y }; // BL
    out[1] = { b.min.X, b.max.Y }; // TL
    out[2] = { b.max.X, b.max.Y }; // TR
    out[3] = { b.max.X, b.min.Y }; // BR
}


bool DynCol::Overlap(const Triangle& tri, const AABB& box) {
    // Build lists of axes (triangle edge normals + box axes)
    // SAT: if any axis separates the projections, shapes do NOT overlap.
    Vector2D triVerts[3] = { tri.v0, tri.v1, tri.v2 };
    Vector2D triEdges[3] = {
        tri.v1 - tri.v0,
        tri.v2 - tri.v1,
        tri.v0 - tri.v2
    };

    Vector2D axes[5];
    // Triangle edge normals (perp vectors)
    axes[0] = perp(triEdges[0]);
    axes[1] = perp(triEdges[1]);
    axes[2] = perp(triEdges[2]);
    // AABB axes (x and y) — sufficient for a box
    axes[3] = Vector2D(1.0f, 0.0f);
    axes[4] = Vector2D(0.0f, 1.0f);

    // Box corners
    Vector2D boxVerts[4];
    boxCorners(box, boxVerts);

    // Test all axes
    for (int i = 0; i < 5; ++i) {
        const Vector2D& axis = axes[i];

        float tmin, tmax;
        projectPolygonOnAxis(triVerts, 3, axis, tmin, tmax);

        float bmin, bmax;
        projectPolygonOnAxis(boxVerts, 4, axis, bmin, bmax);

        // If projections are disjoint on any axis, no overlap
        if (tmax < bmin || bmax < tmin) {
            return false;
        }
    }
    // No separating axis found -> overlap
    return true;
}

// discrete means at the moment current frame
// discrete AABB vs AABB 
bool DynCol::Overlap(const AABB& A, const AABB& B, Manifold* m) {
    const float dx1 = B.min.X - A.max.X; // separation if B is right of A
    const float dx2 = A.min.X - B.max.X; // separation if A is right of B
    const float dy1 = B.min.Y - A.max.Y;
    const float dy2 = A.min.Y - B.max.Y;

    if (dx1 > 0.f || dx2 > 0.f || dy1 > 0.f || dy2 > 0.f) return false;

    if (!m) return true;

    // minimal translation along axis of least penetration
    float px = std::min(A.max.X - B.min.X, B.max.X - A.min.X);
    float py = std::min(A.max.Y - B.min.Y, B.max.Y - A.min.Y);
    if (px < py) {
        m->normal = { (A.max.X + A.min.X < B.max.X + B.min.X) ? -1.f : 1.f, 0.f };
        m->penetration = px;
    }
    else {
        m->normal = { 0.f, (A.max.Y + A.min.Y < B.max.Y + B.min.Y) ? -1.f : 1.f };
        m->penetration = py;
    }
    m->valid = true;
    return true;
}

//discrete circle vs circle
bool DynCol::Overlap(const Circle& A, const Circle& B, Manifold* m) {
    Vector2D d = { B.c.X - A.c.X, B.c.Y - A.c.Y };
    float r = A.r + B.r;
    float d2 = Len2(d);
    if (d2 > r * r) return false;
    if (!m) return true;

    float dlen = std::sqrt(std::max(d2, 1e-12f));
    m->normal = (dlen > 0.f) ? Vector2D{ d.X / dlen, d.Y / dlen } : Vector2D{ 0.f,1.f };
    m->penetration = r - dlen;
    m->contact = { B.c.X + m->normal.X * B.r, B.c.Y + m->normal.Y * B.r };
    m->valid = true;
    return true;
}

//discrete circle vs AABB
bool DynCol::Overlap(const Circle& A, const AABB& B, Manifold* m) {
    // closest point on box to circle center
    float cx = std::min(std::max(A.c.X, B.min.X), B.max.X);
    float cy = std::min(std::max(A.c.Y, B.min.Y), B.max.Y);
    Vector2D q{ cx, cy };
    Vector2D d{ A.c.X - q.X, A.c.Y - q.Y };

    float d2 = Len2(d);
    if (d2 > A.r * A.r) return false;
    if (!m) return true;

    float dlen = std::sqrt(std::max(d2, 1e-12f));
    Vector2D n = (dlen > 0.f) ? Vector2D{ d.X / dlen, d.Y / dlen } : Vector2D{ 0.f,1.f };

    // If center is inside box, push out along smallest axis
    bool inside = (A.c.X >= B.min.X && A.c.X <= B.max.X && A.c.Y >= B.min.Y && A.c.Y <= B.max.Y);
    if (inside) {
        float dx = std::min(A.c.X - B.min.X, B.max.X - A.c.X);
        float dy = std::min(A.c.Y - B.min.Y, B.max.Y - A.c.Y);
        if (dx < dy) n = { (A.c.X - (B.min.X + B.max.X) * 0.5f) < 0.f ? -1.f : 1.f, 0.f };
        else         n = { 0.f, (A.c.Y - (B.min.Y + B.max.Y) * 0.5f) < 0.f ? -1.f : 1.f };
        m->penetration = (dx < dy ? dx : dy) + A.r; // move circle out to surface
        m->contact = { A.c.X - n.X * A.r, A.c.Y - n.Y * A.r };
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
            Vector2D e{ vs[(i + 1) % vs.size()].X - vs[i].X, vs[(i + 1) % vs.size()].Y - vs[i].Y };
            Vector2D n{ -e.Y, e.X }; // edge normal
            float nlen2 = Len2(n); if (nlen2 < 1e-12f) continue;
            n = { n.X / std::sqrt(nlen2), n.Y / std::sqrt(nlen2) };

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
        for (auto& v : A.verts) { cA.X += v.X; cA.Y += v.Y; }
        for (auto& v : B.verts) { cB.X += v.X; cB.Y += v.Y; }
        cA.X /= (float)A.verts.size(); cA.Y /= (float)A.verts.size();
        cB.X /= (float)B.verts.size(); cB.Y /= (float)B.verts.size();
        Vector2D dir{ cA.X - cB.X, cA.Y - cB.Y };
        if (Dot(dir, bestN) < 0.f) { bestN.X = -bestN.X; bestN.Y = -bestN.Y; }

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
    Vector2D vA{ A_end.X - ((A.min.X + A.max.X) * 0.5f), A_end.Y - ((A.min.Y + A.max.Y) * 0.5f) };
    Vector2D vB{ B_end.X - ((B.min.X + B.max.X) * 0.5f), B_end.Y - ((B.min.Y + B.max.Y) * 0.5f) };
    Vector2D vRel{ vA.X - vB.X, vA.Y - vB.Y };

    // Expand B by Aï¿½s half extents (Minkowski sum) and test point vs expanded box:
    Vector2D hA{ (A.max.X - A.min.X) * 0.5f, (A.max.Y - A.min.Y) * 0.5f };
    DynCol::AABB E; // expanded box with center at B
    Vector2D cB{ (B.min.X + B.max.X) * 0.5f, (B.min.Y + B.max.Y) * 0.5f };
    E.min = { B.min.X - hA.X, B.min.Y - hA.Y };
    E.max = { B.max.X + hA.X, B.max.Y + hA.Y };

    // Start point is Aï¿½s center
    Vector2D p0{ (A.min.X + A.max.X) * 0.5f, (A.min.Y + A.max.Y) * 0.5f };
    Vector2D p1{ p0.X + vRel.X, p0.Y + vRel.Y };

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

    if (!slab(p0.X, p1.X, E.min.X, E.max.X, -1.f, 0.f)) return H;
    if (!slab(p0.Y, p1.Y, E.min.Y, E.max.Y, 0.f, -1.f)) return H;

    H.hit = (tEnter >= 0.f && tEnter <= 1.f);
    if (H.hit) {
        H.toi = tEnter;
        float nlen2 = Len2(nEnter);
        if (nlen2 > 0.f) { float s = 1.f / std::sqrt(nlen2); nEnter.X *= s; nEnter.Y *= s; }
        H.normal = nEnter; // points from B -> A
    }
    return H;
}

//swept circle vs circle
DynCol::SweepHit DynCol::Sweep(const Circle& A, const Vector2D& A_end,
    const Circle& B, const Vector2D& B_end)
{
    SweepHit H;

    Vector2D vA{ A_end.X - A.c.X, A_end.Y - A.c.Y };
    Vector2D vB{ B_end.X - B.c.X, B_end.Y - B.c.Y };
    Vector2D v{ vA.X - vB.X, vA.Y - vB.Y }; // relative motion (A wrt B)

    Vector2D w0{ A.c.X - B.c.X, A.c.Y - B.c.Y }; // initial offset
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
            float L2 = Len2(n); if (L2 > 0.f) { float s = 1.f / std::sqrt(L2); n.X *= s; n.Y *= s; }
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

    Vector2D Ac{ A.c.X + vA.X * tHit, A.c.Y + vA.Y * tHit };
    Vector2D Bc{ B.c.X + vB.X * tHit, B.c.Y + vB.Y * tHit };
    Vector2D n{ Ac.X - Bc.X, Ac.Y - Bc.Y };
    float L2 = Len2(n);
    if (L2 > 0.f) { float s = 1.f / std::sqrt(L2); n.X *= s; n.Y *= s; }
    else n = { 0.f,1.f };
    H.normal = n; // from B -> A at impact
    return H;
}

//swept circle vs AABB
DynCol::SweepHit DynCol::Sweep(const Circle& A, const Vector2D& A_end,
    const AABB& B, const Vector2D& B_end)
{
    (void)B_end;

    // Expand box by circle radius
    AABB E;
    E.min = { B.min.X - A.r, B.min.Y - A.r };
    E.max = { B.max.X + A.r, B.max.Y + A.r };

    // Cast point A.c vs expanded box using the same slab routine as AABB sweep
    // Build a unit AABB with A centered; reuse the AABB sweep by giving half-extents/ZERO
    AABB AA;
    AA.min = { A.c.X, A.c.Y };
    AA.max = { A.c.X, A.c.Y };

    Vector2D Aend = A_end;
    Vector2D Bend = { (B.min.X + B.max.X) * 0.5f, (B.min.Y + B.max.Y) * 0.5f }; // no movement
    return Sweep(AA, Aend, E, Bend);
}
