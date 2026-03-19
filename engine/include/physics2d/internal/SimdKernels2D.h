/* Start Header *****************************************************************/
/*!
\file   SimdKernels2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief SIMD-accelerated math kernels with scalar fallback for physics hot paths.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_INTERNAL_SIMDKERNELS2D_H
#define ENGINE_PHYSICS2D_INTERNAL_SIMDKERNELS2D_H

#include "physics/Collision.h"
#include <cmath>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define ENGINE_PHYSICS2D_HAS_SSE2 1
#include <immintrin.h>
#else
#define ENGINE_PHYSICS2D_HAS_SSE2 0
#endif

namespace Engine::Physics2D::Internal {
    /**
     * @brief SIMD/scalar AABB overlap test.
     * @param a First AABB.
     * @param b Second AABB.
     * @return True if AABBs overlap.
     */
    inline bool OverlapAABB2D(const Engine::WorldAABB& a, const Engine::WorldAABB& b) {
#if ENGINE_PHYSICS2D_HAS_SSE2
        const __m128 ca = _mm_set_ps(0.0f, 0.0f, a.Center.Y, a.Center.X);
        const __m128 cb = _mm_set_ps(0.0f, 0.0f, b.Center.Y, b.Center.X);
        const __m128 ha = _mm_set_ps(0.0f, 0.0f, a.HalfExtents.Y, a.HalfExtents.X);
        const __m128 hb = _mm_set_ps(0.0f, 0.0f, b.HalfExtents.Y, b.HalfExtents.X);
        const __m128 absd = _mm_andnot_ps(_mm_set1_ps(-0.0f), _mm_sub_ps(cb, ca));
        const __m128 sumh = _mm_add_ps(ha, hb);
        const __m128 cmp = _mm_cmple_ps(absd, sumh);
        return (_mm_movemask_ps(cmp) & 0x3) == 0x3;
#else
        const Vector2D d = b.Center - a.Center;
        return std::abs(d.X) <= (a.HalfExtents.X + b.HalfExtents.X) &&
            std::abs(d.Y) <= (a.HalfExtents.Y + b.HalfExtents.Y);
#endif
    }

    /**
     * @brief SIMD/scalar circle-circle overlap and manifold normal/depth generation.
     * @param a First circle.
     * @param b Second circle.
     * @param outNormal Collision normal from `a` to `b`.
     * @param outDepth Penetration depth.
     * @return True if circles overlap.
     */
    inline bool CircleCircle(
        const Engine::WorldCircle& a,
        const Engine::WorldCircle& b,
        Vector2D& outNormal,
        float& outDepth)
    {
        const float dx = b.Center.X - a.Center.X;
        const float dy = b.Center.Y - a.Center.Y;
#if ENGINE_PHYSICS2D_HAS_SSE2
        const __m128 d = _mm_set_ps(0.0f, 0.0f, dy, dx);
        const __m128 mul = _mm_mul_ps(d, d);
        alignas(16) float tmp[4];
        _mm_store_ps(tmp, mul);
        const float distSq = tmp[0] + tmp[1];
#else
        const float distSq = dx * dx + dy * dy;
#endif
        const float radii = a.Radius + b.Radius;
        if (distSq >= radii * radii) {
            return false;
        }
        const float dist = std::sqrt(distSq);
        if (dist > 1e-6f) {
            outNormal = Vector2D(dx / dist, dy / dist);
        } else {
            outNormal = Vector2D(1.0f, 0.0f);
        }
        outDepth = radii - dist;
        return true;
    }
}

#endif
