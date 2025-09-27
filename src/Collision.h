#pragma once
#include "Math/Vector2D.h"
#include <utility>

class Collision {
public:
    struct LineSegment {
        Vector2D p0{ 0.0f, 0.0f };
        Vector2D p1{ 0.0f, 0.0f };
        Vector2D normal{ 0.0f, 0.0f };  // unit outward normal
    };

    struct Circle {
        Vector2D center{ 0.0f, 0.0f };
        float    radius{ 0.0f };
    };

    struct AABB {
        Vector2D min{ 0.0f, 0.0f }; // bottom-left
        Vector2D max{ 0.0f, 0.0f }; // top-right
    };

    // Builders
    static LineSegment MakeSegment(const Vector2D& p0, const Vector2D& p1);
    static AABB MakeAABB_MinMax(const Vector2D& minPt, const Vector2D& maxPt) {
        return { minPt, maxPt };
    }
    static AABB MakeAABB_CenterSize(const Vector2D& center, const Vector2D& size) {
        Vector2D half = Vector2D(size.X * 0.5f, size.Y * 0.5f);
        return { Vector2D(center.X - half.X, center.Y - half.Y),
                 Vector2D(center.X + half.X, center.Y + half.Y) };
    }

    // Queries / Intersections
    static bool CircleVsSegmentSweep(
        const Circle& circle,
        const Vector2D& intendedEnd,
        const LineSegment& seg,
        Vector2D& outContactPoint,
        Vector2D& outNormal,
        float& outTime,
        bool checkEdges = true);

    static void CircleSegmentResponse(
        const Vector2D& contactPoint,
        const Vector2D& normal,
        Vector2D& inOutIntendedEnd,
        Vector2D& outReflectedDir);

    static bool PointVsSegment(
        const Vector2D& P,
        const Vector2D& S0,
        const Vector2D& S1,
        float tol,
        float* outT,
        Vector2D* outClosest);

    static bool AABBvsAABB(
        const AABB& A,
        const AABB& B,
        Vector2D* outNormal,
        float* outPenetration);

private:
    static bool solveQuadratic(float a, float b, float c, float& t0, float& t1);
    static bool pointSweepHitCircle(const Vector2D& C0, const Vector2D& C1,
        const Vector2D& P, float radius,
        float& tHit, Vector2D& normalAtHit);
};
