/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file    Collision.h
 * 
 * @brief   contain's defintiions of structs and function declarations used
 * in collision.cpp
 */
#ifndef COLLISION_H
#define COLLISION_H

#include "Math/Vector2D.h"
#include <vector>
#include <cmath>
#include <utility>

namespace Engine {
    class Collision {
    public:
        struct LineSegment {
            Vector2D P0{ 0.0f, 0.0f };
            Vector2D P1{ 0.0f, 0.0f };
            Vector2D Normal{ 0.0f, 0.0f };  // unit outward normal
        };

        struct Circle {
            Vector2D Center{ 0.0f, 0.0f };
            float    Radius{ 0.0f };
        };

        struct AABB {
            Vector2D Min{ 0.0f, 0.0f }; // bottom-left
            Vector2D Max{ 0.0f, 0.0f }; // top-right
        };

        struct ConvexPolygon {
            std::vector<Vector2D> Vertices;
        };

        struct Triangle {
            Vector2D V0, V1, V2;
        };

        struct Manifold {
            Vector2D Normal;  
            float Penetration = 0.0f;
            Vector2D Contact; 
            bool Valid = false;
        };

        // Extended contact manifold with multiple points
        struct ContactManifold {
            Vector2D normal;              // Collision normal (A to B)
            float penetration;            // Penetration depth

            // Contact points (up to 2 in 2D)
            Vector2D points[2];           // World-space contact positions
            int pointCount;               // Number of valid points (1 or 2)

            // Feature IDs for contact tracking
            struct FeaturePair {
                uint8_t inEdge1;          // Edge index on shape 1
                uint8_t outEdge1;         // Edge index on shape 1
                uint8_t inEdge2;          // Edge index on shape 2
                uint8_t outEdge2;         // Edge index on shape 2
            };
            FeaturePair ids[2];           // Feature IDs per contact point

            ContactManifold() : normal(0, 0), penetration(0), pointCount(0) {}
        };

        // Clip segment structure for Sutherland-Hodgman
        struct ClipVertex {
            Vector2D v;                   // Vertex position
            uint8_t edgeIndex;            // Which edge this came from
        };


        struct SweepHit {
            float TimeOfImpact = 1.0f; 
            Vector2D Normal;           
            Vector2D ContactA;         
            Vector2D ContactB;        
            bool Hit = false;
        };

        // Builders
        static LineSegment MakeSegment(const Vector2D& p0, const Vector2D& p1);
        static AABB MakeAABBCenterSize(const Vector2D& center, const Vector2D& size);

        // Queries / Intersections
        static bool CircleVsSegmentSweep(
            const Circle& circle,
            const Vector2D& intendedEnd,
            const LineSegment& segment,
            Vector2D& outContactPoint,
            Vector2D& outNormal,
            float& outTime,
            bool checkEdges = true);

        static void CircleSegmentResponse(
            const Vector2D& contactPoint,
            const Vector2D& normal,
            Vector2D& inOutIntendedEnd,
            Vector2D& outReflectedDirection);

        static bool PointVsSegment(
            const Vector2D& p,
            const Vector2D& s0,
            const Vector2D& s1,
            float tolerance,
            float* outTime,
            Vector2D* outClosest);

        static bool AABBvsAABB(
            const AABB& a,
            const AABB& b,
            Vector2D* outNormal,
            float* outPenetration);

        static bool Overlap(const Triangle& tri, const AABB& box);
        static bool Overlap(const AABB& a, const AABB& b, Manifold* m = nullptr);
        static bool Overlap(const Circle& a, const Circle& b, Manifold* m = nullptr);
        static bool Overlap(const Circle& a, const AABB& b, Manifold* m = nullptr);
        static bool Overlap(const ConvexPolygon& a, const ConvexPolygon& b, Manifold* m = nullptr);

        static SweepHit Sweep(const AABB& a, const Vector2D& aEnd,
            const AABB& b, const Vector2D& bEnd);

        static SweepHit Sweep(const Circle& a, const Vector2D& aEnd,
            const Circle& b, const Vector2D& bEnd);

        static SweepHit Sweep(const Circle& a, const Vector2D& aEnd,
            const AABB& b, const Vector2D& bEnd);

        // Box-box manifold generation with clipping
        static ContactManifold GenerateBoxBoxManifold(
            const Vector2D& centerA,
            const Vector2D& halfExtentsA,
            const Vector2D& centerB,
            const Vector2D& halfExtentsB,
            const Vector2D& normal,
            float penetration);

        // Helper: Clip a line segment against a plane
        static int ClipSegmentToLine(
            ClipVertex outVertices[2],
            const ClipVertex inVertices[2],
            const Vector2D& normal,
            float offset,
            uint8_t clipEdge);

        // Helper: Get edge vertices for an AABB face
        static void GetEdgeVertices(
            const Vector2D& center,
            const Vector2D& halfExtents,
            int edgeIndex,
            Vector2D& v1,
            Vector2D& v2);

    private:
        static bool _solveQuadratic(float a, float b, float c, float& t0, float& t1);
        static bool _pointSweepHitCircle(const Vector2D& c0, const Vector2D& c1,
            const Vector2D& p, float radius,
            float& tHit, Vector2D& normalAtHit);
    };
}

#endif
