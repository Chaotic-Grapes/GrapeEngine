#pragma once
/*
  Vector.h
  ----------------------------------------------------------------
  A minimal 2D vector class for game math.

  - Plain-old-data layout: two floats (x, y)
  - Methods for length, normalization, dot, distance, perp, etc.
  - Operator overloads (+, -, *, /, ==, !=, +=, -=, *=, /=, unary -)
  - Epsilon-based equality check available
  - Optional GLM interop:
        #define CE_VECTOR_ENABLE_GLM 1
        (before including this header)
*/

#include <cstddef>  // size_t
#include <cfloat>   // FLT_EPSILON

#if defined(CE_VECTOR_ENABLE_GLM)
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#endif

namespace CE {

    class Vec2 {
    public:
        float x;
        float y;

        // ----------------------------------------------------------------
        // Constructors
        // ----------------------------------------------------------------
        /// Default: zero vector
        Vec2() noexcept;
        /// From components
        Vec2(float x_, float y_) noexcept;
        /// Broadcast scalar to both components
        explicit Vec2(float s) noexcept;

#if defined(CE_VECTOR_ENABLE_GLM)
        /// Construct from glm::vec2
        explicit Vec2(const glm::vec2& g) noexcept;
        /// Convert to glm::vec2
        glm::vec2 to_glm() const noexcept;
#endif

        // ----------------------------------------------------------------
        // Basic queries
        // ----------------------------------------------------------------
        /// Squared length (no sqrt)
        float length_sq() const noexcept;
        /// Length (sqrt)
        float length() const noexcept;
        /// True if both components are finite
        bool is_finite() const noexcept;

        // ----------------------------------------------------------------
        // Normalization
        // ----------------------------------------------------------------
        /// Return a normalized copy (0 -> (0,0))
        Vec2 normalized(float eps = 1e-8f) const noexcept;
        /// Normalize in-place; returns length before normalization
        float normalize(float eps = 1e-8f) noexcept;

        // ----------------------------------------------------------------
        // Dot / distance
        // ----------------------------------------------------------------
        /// Dot product
        float dot(const Vec2& rhs) const noexcept;
        /// Squared distance to another vector
        float distance_sq(const Vec2& rhs) const noexcept;
        /// Distance to another vector
        float distance(const Vec2& rhs) const noexcept;

        // ----------------------------------------------------------------
        // Perpendicular helpers (2D)
        // ----------------------------------------------------------------
        /// Perp clockwise: ( x, y ) -> ( -y, x )
        Vec2 perp_cw() const noexcept;
        /// Perp counter-clockwise: ( x, y ) -> ( y, -x )
        Vec2 perp_ccw() const noexcept;

        // ----------------------------------------------------------------
        // Component-wise ops
        // ----------------------------------------------------------------
        /// Component-wise min
        static Vec2 min(const Vec2& a, const Vec2& b) noexcept;
        /// Component-wise max
        static Vec2 max(const Vec2& a, const Vec2& b) noexcept;
        /// Clamp each component to [lo, hi]
        static Vec2 clamp(const Vec2& v, const Vec2& lo, const Vec2& hi) noexcept;

        // ----------------------------------------------------------------
        // Comparison
        // ----------------------------------------------------------------
        /// Exact compare
        bool operator==(const Vec2& rhs) const noexcept;
        bool operator!=(const Vec2& rhs) const noexcept;
        /// Epsilon compare (component-wise)
        bool equals_eps(const Vec2& rhs, float eps = 1e-6f) const noexcept;

        // ----------------------------------------------------------------
        // Indexing
        // ----------------------------------------------------------------
        float& operator[](std::size_t idx) noexcept;
        const float& operator[](std::size_t idx) const noexcept;

        // ----------------------------------------------------------------
        // Arithmetic (in-place)
        // ----------------------------------------------------------------
        Vec2& operator+=(const Vec2& rhs) noexcept;
        Vec2& operator-=(const Vec2& rhs) noexcept;
        Vec2& operator*=(float s) noexcept;
        Vec2& operator/=(float s) noexcept;

        // ----------------------------------------------------------------
        // Constants
        // ----------------------------------------------------------------
        static const Vec2 Zero;   // (0,0)
        static const Vec2 UnitX;  // (1,0)
        static const Vec2 UnitY;  // (0,1)
    };

    // ------------------------------------------------------------------
    // Free operators (value-returning)
    // ------------------------------------------------------------------
    inline Vec2 operator+(Vec2 a, const Vec2& b) noexcept { a += b; return a; }
    inline Vec2 operator-(Vec2 a, const Vec2& b) noexcept { a -= b; return a; }
    inline Vec2 operator-(const Vec2& v) noexcept { return Vec2(-v.x, -v.y); }
    inline Vec2 operator*(Vec2 v, float s) noexcept { v *= s; return v; }
    inline Vec2 operator*(float s, Vec2 v) noexcept { v *= s; return v; }
    inline Vec2 operator/(Vec2 v, float s) noexcept { v /= s; return v; }

    // ------------------------------------------------------------------
    // Utility free functions (nice to have symmetrical API)
    // ------------------------------------------------------------------
    inline float Dot(const Vec2& a, const Vec2& b) noexcept { return a.dot(b); }
    inline float Length(const Vec2& v) noexcept { return v.length(); }
    inline float LengthSq(const Vec2& v) noexcept { return v.length_sq(); }
    inline float Distance(const Vec2& a, const Vec2& b) noexcept { return a.distance(b); }
    inline float DistanceSq(const Vec2& a, const Vec2& b) noexcept { return a.distance_sq(b); }
    inline Vec2  Normalize(const Vec2& v) noexcept { return v.normalized(); }

} // namespace CE
