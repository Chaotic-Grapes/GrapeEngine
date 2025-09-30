#ifndef DYNAMICCOLLISION_H
#define DYNAMICCOLLISION_H

#include <vector>
#include "Math/Vector2D.h"  

namespace DynCol {

    struct AABB { Vector2D min, max; };        
    struct Circle { Vector2D c; float r; };
    struct Segment { Vector2D p0, p1; };       
    struct ConvexPolygon { std::vector<Vector2D> verts; }; 

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
