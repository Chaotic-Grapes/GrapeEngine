#include "Vector2D.h"
#include <cmath>    // std::sqrt, std::isfinite

namespace CE {

    // --------------------------------------------------------------
    // Static constants
    // --------------------------------------------------------------
    const Vec2 Vec2::Zero = Vec2(0.0f, 0.0f);
    const Vec2 Vec2::UnitX = Vec2(1.0f, 0.0f);
    const Vec2 Vec2::UnitY = Vec2(0.0f, 1.0f);

    // --------------------------------------------------------------
    // Ctors
    // --------------------------------------------------------------
    Vec2::Vec2() noexcept : x(0.0f), y(0.0f) {}
    Vec2::Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}
    Vec2::Vec2(float s) noexcept : x(s), y(s) {}

#if defined(CE_VECTOR_ENABLE_GLM)
    Vec2::Vec2(const glm::vec2& g) noexcept : x(g.x), y(g.y) {}
    glm::vec2 Vec2::to_glm() const noexcept { return glm::vec2(x, y); }
#endif

    // --------------------------------------------------------------
    // Queries
    // --------------------------------------------------------------
    float Vec2::length_sq() const noexcept { return x * x + y * y; }

    float Vec2::length() const noexcept {
        // Use sqrtf for float; include <cmath>
        return std::sqrt(length_sq());
    }

    bool Vec2::is_finite() const noexcept {
        // std::isfinite is in <cmath> (C++11)
        return std::isfinite(x) && std::isfinite(y);
    }

    // --------------------------------------------------------------
    // Normalization
    // --------------------------------------------------------------
    Vec2 Vec2::normalized(float eps) const noexcept {
        float L2 = length_sq();
        if (L2 <= eps * eps) return Vec2(0.0f, 0.0f);
        float invL = 1.0f / std::sqrt(L2);
        return Vec2(x * invL, y * invL);
    }

    float Vec2::normalize(float eps) noexcept {
        float L2 = length_sq();
        if (L2 <= eps * eps) {
            x = 0.0f; y = 0.0f;
            return 0.0f;
        }
        float L = std::sqrt(L2);
        float invL = 1.0f / L;
        x *= invL; y *= invL;
        return L;
    }

    // --------------------------------------------------------------
    // Dot / distance
    // --------------------------------------------------------------
    float Vec2::dot(const Vec2& rhs) const noexcept {
        return x * rhs.x + y * rhs.y;
    }

    float Vec2::distance_sq(const Vec2& rhs) const noexcept {
        float dx = rhs.x - x;
        float dy = rhs.y - y;
        return dx * dx + dy * dy;
    }

    float Vec2::distance(const Vec2& rhs) const noexcept {
        return std::sqrt(distance_sq(rhs));
    }

    // --------------------------------------------------------------
    // Perpendiculars
    // --------------------------------------------------------------
    Vec2 Vec2::perp_cw() const noexcept {
        // (x, y) -> (-y, x)
        return Vec2(-y, x);
    }

    Vec2 Vec2::perp_ccw() const noexcept {
        // (x, y) -> (y, -x)
        return Vec2(y, -x);
    }

    // --------------------------------------------------------------
    // Component-wise ops
    // --------------------------------------------------------------
    Vec2 Vec2::min(const Vec2& a, const Vec2& b) noexcept {
        return Vec2(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y);
    }

    Vec2 Vec2::max(const Vec2& a, const Vec2& b) noexcept {
        return Vec2(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y);
    }

    Vec2 Vec2::clamp(const Vec2& v, const Vec2& lo, const Vec2& hi) noexcept {
        float cx = v.x < lo.x ? lo.x : (v.x > hi.x ? hi.x : v.x);
        float cy = v.y < lo.y ? lo.y : (v.y > hi.y ? hi.y : v.y);
        return Vec2(cx, cy);
    }

    // --------------------------------------------------------------
    // Comparison
    // --------------------------------------------------------------
    bool Vec2::operator==(const Vec2& rhs) const noexcept {
        return x == rhs.x && y == rhs.y;
    }

    bool Vec2::operator!=(const Vec2& rhs) const noexcept {
        return !(*this == rhs);
    }

    bool Vec2::equals_eps(const Vec2& rhs, float eps) const noexcept {
        float dx = (x > rhs.x) ? (x - rhs.x) : (rhs.x - x);
        float dy = (y > rhs.y) ? (y - rhs.y) : (rhs.y - y);
        return dx <= eps && dy <= eps;
    }

    // --------------------------------------------------------------
    // Indexing
    // --------------------------------------------------------------
    float& Vec2::operator[](std::size_t idx) noexcept {
        // 0 -> x, 1 -> y; undefined for others (assert in debug if desired)
        return idx == 0 ? x : y;
    }

    const float& Vec2::operator[](std::size_t idx) const noexcept {
        return idx == 0 ? x : y;
    }

    // --------------------------------------------------------------
    // Arithmetic (in-place)
    // --------------------------------------------------------------
    Vec2& Vec2::operator+=(const Vec2& rhs) noexcept {
        x += rhs.x; y += rhs.y; return *this;
    }

    Vec2& Vec2::operator-=(const Vec2& rhs) noexcept {
        x -= rhs.x; y -= rhs.y; return *this;
    }

    Vec2& Vec2::operator*=(float s) noexcept {
        x *= s; y *= s; return *this;
    }

    Vec2& Vec2::operator/=(float s) noexcept {
        // No divide-by-zero guard; caller’s responsibility
        float inv = 1.0f / s;
        x *= inv; y *= inv; return *this;
    }

} // namespace CE
