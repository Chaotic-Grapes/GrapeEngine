#ifndef DYNAMICCOLLISION_H
#define DYNAMICCOLLISION_H

#include <vector>
#include "Math/Vector2D.h"  
#include <cmath>

namespace DynCol {
    
    //shape types
    struct AABB { Vector2D min, max; };        
    struct Circle { Vector2D c; float r; };
    struct Segment { Vector2D p0, p1; };       
    struct ConvexPolygon { std::vector<Vector2D> verts; }; 

    struct Triangle { Vector2D v0, v1, v2; };

    inline Triangle MakeEquilateral(const Vector2D& c, float side, float angleRad) {
        const float h = (std::sqrt(3.0f) * 0.5f) * side;
        const float hb = 0.5f * side;
        const float hh = (2.0f / 3.0f) * h;  // centroid -> top
        const float lh = (1.0f / 3.0f) * h;  // centroid -> base line

        Vector2D v0_local(0.0f, hh);
        Vector2D v1_local(-hb, -lh);
        Vector2D v2_local(hb, -lh);

        const float cs = std::cos(angleRad);
        const float sn = std::sin(angleRad);
        auto rot = [&](const Vector2D& p) { return Vector2D(p.X * cs - p.Y * sn, p.X * sn + p.Y * cs); };

        Triangle t;
        t.v0 = c + rot(v0_local);
        t.v1 = c + rot(v1_local);
        t.v2 = c + rot(v2_local);
        return t;
    }

    // Contact for discrete overlap
    struct Manifold {
        Vector2D normal;  // points from B -> A
        float penetration = 0.0f;
        Vector2D contact; // optional representative point
        bool valid = false;
    };

    // Sweep result for Time of impact queries
    struct SweepHit {
        float toi = 1.0f;         // time of impact in [0,1]
        Vector2D normal;          // unit normal from B -> A at impact
        Vector2D contactA;        // optional
        Vector2D contactB;        // optional
        bool hit = false;
    };

    // discretes
    bool Overlap(const Triangle& tri, const AABB& box);
    bool Overlap(const AABB& A, const AABB& B, Manifold* m = nullptr);
    bool Overlap(const Circle& A, const Circle& B, Manifold* m = nullptr);
    bool Overlap(const Circle& A, const AABB& B, Manifold* m = nullptr);
    bool Overlap(const ConvexPolygon& A, const ConvexPolygon& B, Manifold* m = nullptr);

    //swept 
    SweepHit Sweep(const AABB& A, const Vector2D& A_end,
        const AABB& B, const Vector2D& B_end);

    SweepHit Sweep(const Circle& A, const Vector2D& A_end,
        const Circle& B, const Vector2D& B_end);

    SweepHit Sweep(const Circle& A, const Vector2D& A_end,
        const AABB& B, const Vector2D& B_end);

} // namespace DynCol

#endif
