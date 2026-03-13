/* Start Header *****************************************************************/
/*!
\file   Collision.h
\author Dalton Koh
\par    d.koh@digipen.edu
\brief
Declares collision data types and query functions used by the physics runtime.

Description
- defines world space shape structs for collision checks
- declares overlap and sweep tests for common 2d shapes
- provides manifold data for collision response
- includes helper routines for clipping and edge extraction

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef COLLISION_H
#define COLLISION_H

#include "Math/Vector2D.h"
#include <vector>
#include <cmath>
#include <utility>

namespace Engine {
    // world space circle used by collision systems
    
    struct WorldCircle {
        // center in world space
        Vector2D Center;
        // radius in world units
        float Radius;
    };

    // world space aabb used by collision systems
    struct WorldAABB {
        // center in world space
        Vector2D Center;
        // half size in world units
        Vector2D HalfExtents;
    };

    // world space obb used for rotated box checks
    struct WorldOBB {
        // center in world space
        Vector2D Center;
        // half size in world units
        Vector2D HalfExtents;
        // rotation in radians
        float Rotation = 0.0f;
        // local x axis in world space
        Vector2D AxisX{ 1.0f, 0.0f };
        // local y axis in world space
        Vector2D AxisY{ 0.0f, 1.0f };
    };

    // static collision helper class
    class Collision {
    public:
        // line segment with cached outward normal
        struct LineSegment {
            // segment start point
            Vector2D P0{ 0.0f, 0.0f };
            // segment end point
            Vector2D P1{ 0.0f, 0.0f };
            // unit outward normal
            Vector2D Normal{ 0.0f, 0.0f };
        };

        // circle primitive for queries
        struct Circle {
            // center position
            Vector2D Center{ 0.0f, 0.0f };
            // radius value
            float Radius{ 0.0f };
        };

        // axis aligned box primitive
        struct AABB {
            // minimum corner
            Vector2D Min{ 0.0f, 0.0f };
            // maximum corner
            Vector2D Max{ 0.0f, 0.0f };
        };

        // convex polygon primitive
        struct ConvexPolygon {
            // ordered polygon vertices
            std::vector<Vector2D> Vertices;
        };

        // triangle primitive
        struct Triangle {
            // first vertex
            Vector2D V0, V1, V2;
        };

        // simple manifold with one contact
        struct Manifold {
            // normal from shape a to shape b
            Vector2D Normal;  
            // penetration depth
            float Penetration = 0.0f;
            // world contact point
            Vector2D Contact; 
            // result validity flag
            bool Valid = false;
        };

        // contact manifold with up to two contacts
        struct ContactManifold {
            // collision normal from shape a to shape b
            Vector2D normal;
            // penetration depth
            float penetration;

            // world space contact positions
            Vector2D points[2];
            // number of valid points
            int pointCount;

            // feature pair used for contact tracking
            struct FeaturePair {
                // entering edge on shape one
                uint8_t inEdge1;
                // exiting edge on shape one
                uint8_t outEdge1;
                // entering edge on shape two
                uint8_t inEdge2;
                // exiting edge on shape two
                uint8_t outEdge2;
            };
            // feature ids for each contact point
            FeaturePair ids[2];

            // default manifold values
            ContactManifold() : normal(0, 0), penetration(0), pointCount(0) {}
        };

        // clip vertex used by segment clipping routines
        struct ClipVertex {
            // vertex position
            Vector2D v;
            // source edge index
            uint8_t edgeIndex;
        };

        // sweep test output data
        struct SweepHit {
            // time of impact in range zero to one
            float TimeOfImpact = 1.0f;
            // collision normal
            Vector2D Normal;
            // contact point on shape a
            Vector2D ContactA;
            // contact point on shape b
            Vector2D ContactB;
            // true when a hit occurred
            bool Hit = false;
        };

        // build a segment from two points
        static LineSegment MakeSegment(const Vector2D& p0, const Vector2D& p1);

        // build an aabb from center and size
        static AABB MakeAABBCenterSize(const Vector2D& center, const Vector2D& size);

        // test swept circle against line segment
        static bool CircleVsSegmentSweep(
            const Circle& circle,
            const Vector2D& intendedEnd,
            const LineSegment& segment,
            Vector2D& outContactPoint,
            Vector2D& outNormal,
            float& outTime,
            bool checkEdges = true);

        // compute response after circle segment contact
        static void CircleSegmentResponse(
            const Vector2D& contactPoint,
            const Vector2D& normal,
            Vector2D& inOutIntendedEnd,
            Vector2D& outReflectedDirection);

        // test point against segment with tolerance
        static bool PointVsSegment(
            const Vector2D& p,
            const Vector2D& s0,
            const Vector2D& s1,
            float tolerance,
            float* outTime,
            Vector2D* outClosest);

        // test overlap between two aabbs
        static bool AABBvsAABB(
            const AABB& a,
            const AABB& b,
            Vector2D* outNormal,
            float* outPenetration);

        // overlap test triangle vs aabb
        static bool Overlap(const Triangle& tri, const AABB& box);

        // overlap test aabb vs aabb
        static bool Overlap(const AABB& a, const AABB& b, Manifold* m = nullptr);

        // overlap test circle vs circle
        static bool Overlap(const Circle& a, const Circle& b, Manifold* m = nullptr);

        // overlap test circle vs aabb
        static bool Overlap(const Circle& a, const AABB& b, Manifold* m = nullptr);

        // overlap test convex polygon vs convex polygon
        static bool Overlap(const ConvexPolygon& a, const ConvexPolygon& b, Manifold* m = nullptr);

        // swept test aabb vs aabb
        static SweepHit Sweep(const AABB& a, const Vector2D& aEnd,
            const AABB& b, const Vector2D& bEnd);

        // swept test circle vs circle
        static SweepHit Sweep(const Circle& a, const Vector2D& aEnd,
            const Circle& b, const Vector2D& bEnd);

        // swept test circle vs aabb
        static SweepHit Sweep(const Circle& a, const Vector2D& aEnd,
            const AABB& b, const Vector2D& bEnd);

        // build box box contact manifold
        static ContactManifold GenerateBoxBoxManifold(
            const Vector2D& centerA,
            const Vector2D& halfExtentsA,
            const Vector2D& centerB,
            const Vector2D& halfExtentsB,
            const Vector2D& normal,
            float penetration);

        // clip a segment against a line plane
        static int ClipSegmentToLine(
            ClipVertex outVertices[2],
            const ClipVertex inVertices[2],
            const Vector2D& normal,
            float offset,
            uint8_t clipEdge);

        // get edge vertices for one aabb face
        static void GetEdgeVertices(
            const Vector2D& center,
            const Vector2D& halfExtents,
            int edgeIndex,
            Vector2D& v1,
            Vector2D& v2);

    private:
        // solve quadratic roots used by sweep math
        static bool _solveQuadratic(float a, float b, float c, float& t0, float& t1);

        // sweep helper for point against expanded circle
        static bool _pointSweepHitCircle(const Vector2D& c0, const Vector2D& c1,
            const Vector2D& p, float radius,
            float& tHit, Vector2D& normalAtHit);
    };
}

#endif
